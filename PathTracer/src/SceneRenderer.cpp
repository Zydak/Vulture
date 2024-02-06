#include "pch.h"
#include "SceneRenderer.h"

#include "imgui.h"
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include "CameraScript.h"

SceneRenderer::SceneRenderer()
{
	Vulture::Ref<Vulture::Image> tempImage = std::make_shared<Vulture::Image>("../Vulture/assets/black.hdr");

	Vulture::Image::CreateInfo imageInfo = {};
	imageInfo.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	imageInfo.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
	imageInfo.Height = 1024;
	imageInfo.Width = 1024;
	imageInfo.LayerCount = 6;
	imageInfo.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	imageInfo.SamplerInfo = {};
	imageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.Type = Vulture::Image::ImageType::Cubemap;
	imageInfo.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	m_Skybox = std::make_shared<Vulture::Image>(imageInfo);

	Vulture::Renderer::EnvMapToCubemapPass(tempImage, m_Skybox);

	CreateRenderPasses();

	CreateFramebuffers();
	m_PresentedImage = m_PathTracingImage;

	CreateDescriptorSets();
	CreatePipelines();

	m_Denoiser = std::make_shared<Vulture::Denoiser>();
	m_Denoiser->AllocateBuffers(Vulture::Renderer::GetSwapchain().GetSwapchainExtent());

	VkFenceCreateInfo createInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	vkCreateFence(Vulture::Device::GetDevice(), &createInfo, nullptr, &m_DenoiseFence);

	Vulture::Renderer::RenderImGui([this](){ImGuiPass(); });
	m_TotalTimer.Reset();
}

SceneRenderer::~SceneRenderer()
{
	vkDestroyFence(Vulture::Device::GetDevice(), m_DenoiseFence, nullptr);
}

void SceneRenderer::Render(Vulture::Scene& scene)
{
	m_CurrentSceneRendered = &scene;

	if (Vulture::Renderer::BeginFrame())
	{
		UpdateDescriptorSetsData();
		RayTrace(glm::vec4(0.1f));

		// Run denoised if requested
		if (m_RunDenoising)
		{
			Denoise();
			m_RunDenoising = false;
		}

		Vulture::Renderer::FramebufferCopyPassImGui(m_HDRDescriptorSet);
		
		if (!Vulture::Renderer::EndFrame())
			RecreateResources();
	}
	else
	{
		RecreateResources();
	}
}

void SceneRenderer::RayTrace(const glm::vec4& clearColor)
{
	m_PushContantRay.ClearColor = clearColor;
	m_PushContantRay.maxDepth = m_MaxRayDepth;
	m_PushContantRay.FocalLength = m_FocalLength;
	m_PushContantRay.DoFStrength = m_DoFStrength;

	static glm::mat4 previousMat{ 0.0f };
	if (previousMat != m_CurrentSceneRendered->GetMainCamera()->ViewMat) // if camera moved
	{
		ResetFrame();
		previousMat = m_CurrentSceneRendered->GetMainCamera()->ViewMat;
	}
	else
	{
		if (m_CurrentSamplesPerPixel >= (uint32_t)m_MaxSamplesPerPixel)
		{
			// tone map if finished
			if (!m_ToneMapped)
			{
				Vulture::Renderer::BloomPass(m_PresentedImage, 8);
				m_PresentedImage->TransitionImageLayout(m_PresentedImage->GetLayout(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, Vulture::Renderer::GetCurrentCommandBuffer());
				Vulture::Renderer::ToneMapPass(m_ToneMapDescriptorSet, m_PresentedImage, m_Exposure);
				m_ToneMapped = true;
			}
			
			return;
		}

		m_PushContantRay.frame++;
		m_CurrentSamplesPerPixel += 15;
	}
	
	m_Time = m_TotalTimer.ElapsedSeconds();

	m_RtPipeline.Bind(Vulture::Renderer::GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	m_RayTracingDescriptorSet->Bind(
		0,
		m_RtPipeline.GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);
	m_GlobalDescriptorSets[Vulture::Renderer::GetCurrentFrameIndex()]->Bind(
		1,
		m_RtPipeline.GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);

	vkCmdPushConstants(Vulture::Renderer::GetCurrentCommandBuffer(), m_RtPipeline.GetPipelineLayout(),
		VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
		0, sizeof(PushConstantRay), &m_PushContantRay);

	Vulture::Device::vkCmdTraceRaysKHR(
		Vulture::Renderer::GetCurrentCommandBuffer(), 
		&m_SBT.GetRGenRegion(),
		&m_SBT.GetMissRegion(),
		&m_SBT.GetHitRegion(),
		&m_SBT.GetCallRegion(),
		(uint32_t)m_PathTracingImage->GetImageSize().width,
		(uint32_t)m_PathTracingImage->GetImageSize().height,
		1
	);
}

void SceneRenderer::DrawGBuffer()
{
	auto view = m_CurrentSceneRendered->GetRegistry().view<Vulture::ModelComponent, Vulture::TransformComponent>();
	std::vector<VkClearValue> clearColors;
	clearColors.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
	clearColors.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
	clearColors.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
	VkClearValue clearVal{};
	clearVal.depthStencil = { 1.0f, 1 };
	clearColors.push_back(clearVal);

	m_GBufferPass.SetRenderTarget(&(*m_GBufferFramebuffer));
	m_GBufferPass.BeginRenderPass(clearColors);
	m_GBufferFramebuffer->GetColorImageNoVk(0)->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_GBufferFramebuffer->GetColorImageNoVk(1)->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_GBufferFramebuffer->GetColorImageNoVk(2)->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	m_GlobalDescriptorSets[Vulture::Renderer::GetCurrentFrameIndex()]->Bind
	(
		0,
		m_GBufferPass.GetPipeline().GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);


	for (auto& entity : view)
	{
		auto& [modelComp, TransformComp] = m_CurrentSceneRendered->GetRegistry().get<Vulture::ModelComponent, Vulture::TransformComponent>(entity);
		
		std::vector<Vulture::Ref<Vulture::Mesh>> meshes = modelComp.Model->GetMeshes();
		for (uint32_t i = 0; i < modelComp.Model->GetMeshCount(); i++)
		{
			PushConstantGBuffer push;
			push.Material = modelComp.Model->GetMaterial(i);
			push.Model = TransformComp.transform.GetMat4();

			vkCmdPushConstants(
				Vulture::Renderer::GetCurrentCommandBuffer(),
				m_GBufferPass.GetPipeline().GetPipelineLayout(),
				VK_SHADER_STAGE_VERTEX_BIT,
				0,
				sizeof(PushConstantGBuffer),
				&push
			);

			meshes[i]->Bind(Vulture::Renderer::GetCurrentCommandBuffer());
			meshes[i]->Draw(Vulture::Renderer::GetCurrentCommandBuffer(), 1, 0);
		}
	}

	m_GBufferPass.EndRenderPass();
}

void SceneRenderer::Denoise()
{
	VkCommandBuffer cmd = Vulture::Renderer::GetCurrentCommandBuffer();

	// Draw Albedo, Roughness, Metallness, Normal into GBuffer
	DrawGBuffer();

	VkMemoryBarrier memBarrierRead{};
	memBarrierRead.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memBarrierRead.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	memBarrierRead.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	// Wait for GBuffer to finish
	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		1, &memBarrierRead,
		0, nullptr,
		0, nullptr);

	// copy images to cuda buffers
	std::vector<Vulture::Image*> vec = 
	{
		&(*m_PathTracingImage), // Path Tracing Result
		const_cast<Vulture::Image*>((m_GBufferFramebuffer->GetColorImageNoVk(GBufferImage::Albedo))), // Albedo
		const_cast<Vulture::Image*>((m_GBufferFramebuffer->GetColorImageNoVk(GBufferImage::Normal))) // Normal
	};
	m_Denoiser->ImageToBuffer(cmd, vec);

	vkEndCommandBuffer(cmd);

	VkCommandBufferSubmitInfoKHR cmd_buf_info{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR };
	cmd_buf_info.commandBuffer = cmd;

	// Increment for signaling
	m_DenoiseFenceValue++;

	VkSemaphoreSubmitInfoKHR signal_semaphore{ VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR };
	signal_semaphore.semaphore = m_Denoiser->GetTLSemaphore();
	signal_semaphore.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
	signal_semaphore.value = m_DenoiseFenceValue;

	VkSubmitInfo2KHR submits{ VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR };
	submits.commandBufferInfoCount = 1;
	submits.pCommandBufferInfos = &cmd_buf_info;

	submits.signalSemaphoreInfoCount = 1;
	submits.pSignalSemaphoreInfos = &signal_semaphore;

	// Submit command buffer so that resources can be used by Optix and CUDA
	vkResetFences(Vulture::Device::GetDevice(), 1, &m_DenoiseFence);
	vkQueueSubmit2(Vulture::Device::GetGraphicsQueue(), 1, &submits, m_DenoiseFence);

	// Run Denoiser
	m_Denoiser->DenoiseImageBuffer(m_DenoiseFenceValue, 0.0f);

	// Wait for Optix and CUDA to finish
	VkResult res = vkWaitForFences(Vulture::Device::GetDevice(), 1, &m_DenoiseFence, VK_TRUE, UINT64_MAX);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	
	// Start recording command buffer
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Copy Optix denoised image to m_DenoisedImage
	m_Denoiser->BufferToImage(cmd, &(*m_DenoisedImage));

	VkMemoryBarrier memBarrier{};
	memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	// Wait for copying to finish
	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		1, &memBarrier,
		0, nullptr,
		0, nullptr);
}

void SceneRenderer::ResetFrame()
{
	m_PushContantRay.frame = -1;
	m_CurrentSamplesPerPixel = 0;
	m_TotalTimer.Reset();
	m_ToneMapped = false;
}

void SceneRenderer::RecreateResources()
{
	ResetFrame();
	m_PushContantRay.frame -= 1;

	CreateFramebuffers();
	CreatePipelines();

	FixCameraAspectRatio();

	m_Denoiser->AllocateBuffers(Vulture::Renderer::GetSwapchain().GetSwapchainExtent());

	if (m_ShowDenoised)
	{
		m_PresentedImage = m_DenoisedImage;
	}
	else
	{
		m_PresentedImage = m_PathTracingImage;
	}

	RecreateDescriptorSets();
}

void SceneRenderer::FixCameraAspectRatio()
{
	float newAspectRatio = (float)Vulture::Renderer::GetSwapchain().GetSwapchainExtent().width / (float)Vulture::Renderer::GetSwapchain().GetSwapchainExtent().height;
	auto view = m_CurrentSceneRendered->GetRegistry().view<Vulture::CameraComponent>();
	for (auto entity : view)
	{
		auto& cameraCp = view.get<Vulture::CameraComponent>(entity);
		cameraCp.SetPerspectiveMatrix(45.0f, newAspectRatio, 0.1f, 100.0f);
	}
}

void SceneRenderer::CreateRenderPasses()
{
	// HDR Pass
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference colorAttachmentRef = {};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = nullptr;

		VkSubpassDependency dependency1 = {};
		dependency1.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency1.dstSubpass = 0;
		dependency1.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dependency1.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency1.srcAccessMask = 0;
		dependency1.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkSubpassDependency dependency2 = {};
		dependency2.srcSubpass = 0;
		dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
		dependency2.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependency2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		std::vector<VkSubpassDependency> dependencies{ dependency1, dependency2 };

		std::vector<VkAttachmentDescription> attachments = { colorAttachment };
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = (uint32_t)attachments.size();
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = (uint32_t)dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();

		m_HDRPass.CreateRenderPass(renderPassInfo);
	}

	// GBuffer Pass
	{
		VkAttachmentDescription albedoAttachment = {};
		albedoAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		albedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		albedoAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		albedoAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		albedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference albedoAttachmentRef = {};
		albedoAttachmentRef.attachment = 0;
		albedoAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription normalAttachment = {};
		normalAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference normalAttachmentRef = {};
		normalAttachmentRef.attachment = 1;
		normalAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription rougnessMetallnessAttachment = {};
		rougnessMetallnessAttachment.format = VK_FORMAT_R8G8_UNORM;
		rougnessMetallnessAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		rougnessMetallnessAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		rougnessMetallnessAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		rougnessMetallnessAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		rougnessMetallnessAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		rougnessMetallnessAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		rougnessMetallnessAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference RoghnessMetallnessAttachmentRef = {};
		RoghnessMetallnessAttachmentRef.attachment = 2;
		RoghnessMetallnessAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = Vulture::Swapchain::FindDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 3;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		std::vector<VkAttachmentReference> references { albedoAttachmentRef, normalAttachmentRef, RoghnessMetallnessAttachmentRef };

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 3;
		subpass.pColorAttachments = references.data();
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		VkSubpassDependency dependency1 = {};
		dependency1.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency1.dstSubpass = 0;
		dependency1.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dependency1.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency1.srcAccessMask = 0;
		dependency1.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkSubpassDependency dependency2 = {};
		dependency2.srcSubpass = 0;
		dependency2.dstSubpass = VK_SUBPASS_EXTERNAL;
		dependency2.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency2.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependency2.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		std::vector<VkSubpassDependency> dependencies{ dependency1, dependency2 };

		std::vector<VkAttachmentDescription> attachments { albedoAttachment, normalAttachment, rougnessMetallnessAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = (uint32_t)attachments.size();
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = (uint32_t)dependencies.size();
		renderPassInfo.pDependencies = dependencies.data();

		m_GBufferPass.CreateRenderPass(renderPassInfo);
	}
}

void SceneRenderer::CreateDescriptorSets()
{
	m_GlobalDescriptorSets.clear();

	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		
		m_PresentedImage = m_PathTracingImage;
		m_HDRDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
		m_HDRDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin });
		m_HDRDescriptorSet->AddImageSampler(
			0,
			m_PresentedImage->GetSamplerHandle(),
			m_PresentedImage->GetImageView(),
			VK_IMAGE_LAYOUT_GENERAL
		);
		m_HDRDescriptorSet->Build();
	}

	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

		m_ToneMapDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
		m_ToneMapDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin });
		m_ToneMapDescriptorSet->AddImageSampler(
			0,
			m_PresentedImage->GetSamplerHandle(),
			m_PresentedImage->GetImageView(),
			VK_IMAGE_LAYOUT_GENERAL 
		);
		m_ToneMapDescriptorSet->Build();
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MISS_BIT_KHR };

		m_GlobalDescriptorSets.push_back(std::make_shared<Vulture::DescriptorSet>());
		m_GlobalDescriptorSets[i]->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
		m_GlobalDescriptorSets[i]->AddUniformBuffer(0, sizeof(GlobalUbo));

		m_GlobalDescriptorSets[i]->AddImageSampler(
			1,
			m_Skybox->GetSamplerHandle(),
			m_Skybox->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		m_GlobalDescriptorSets[i]->Build();
	}
}

void SceneRenderer::CreateRayTracingDescriptorSets(Vulture::Scene& scene)
{
	{
		uint32_t texturesCount = scene.GetMeshCount();
		Vulture::DescriptorSetLayout::Binding bin0{ 0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin2{ 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin3{ 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin4{ 4, texturesCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin5{ 5, texturesCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin6{ 6, texturesCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin7{ 7, texturesCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

		m_RayTracingDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
		m_RayTracingDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin0, bin1, bin2, bin3, bin4, bin5, bin6, bin7 });

		VkAccelerationStructureKHR tlas = scene.GetAccelerationStructure()->GetTlas().Accel;
		VkWriteDescriptorSetAccelerationStructureKHR asInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		asInfo.accelerationStructureCount = 1;
		asInfo.pAccelerationStructures = &tlas;

		m_RayTracingDescriptorSet->AddAccelerationStructure(0, asInfo);
		m_RayTracingDescriptorSet->AddImageSampler(1, m_PathTracingImage->GetSamplerHandle(), m_PathTracingImage->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
		m_RayTracingDescriptorSet->AddStorageBuffer(2, sizeof(MeshAdresses) * 20'000, true);
		m_RayTracingDescriptorSet->AddStorageBuffer(3, sizeof(Vulture::Material) * 20'000, true);

		auto view = scene.GetRegistry().view<Vulture::ModelComponent>();
		for (auto& entity : view)
		{
			auto& modelComp = scene.GetRegistry().get<Vulture::ModelComponent>(entity);
			for (int j = 0; j < (int)modelComp.Model->GetAlbedoTextureCount(); j++)
			{
				m_RayTracingDescriptorSet->AddImageSampler(
					4,
					modelComp.Model->GetAlbedoTexture(j)->GetSamplerHandle(),
					modelComp.Model->GetAlbedoTexture(j)->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				);
			}
			for (int j = 0; j < (int)modelComp.Model->GetNormalTextureCount(); j++)
			{
				m_RayTracingDescriptorSet->AddImageSampler(
					5,
					modelComp.Model->GetNormalTexture(j)->GetSamplerHandle(),
					modelComp.Model->GetNormalTexture(j)->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				);
			}
			for (int j = 0; j < (int)modelComp.Model->GetRoughnessTextureCount(); j++)
			{
				m_RayTracingDescriptorSet->AddImageSampler(
					6,
					modelComp.Model->GetRoughnessTexture(j)->GetSamplerHandle(),
					modelComp.Model->GetRoughnessTexture(j)->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				);
			}
			for (int j = 0; j < (int)modelComp.Model->GetMetallnessTextureCount(); j++)
			{
				m_RayTracingDescriptorSet->AddImageSampler(
					7,
					modelComp.Model->GetMetallnessTexture(j)->GetSamplerHandle(),
					modelComp.Model->GetMetallnessTexture(j)->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				);
			}
		}
		
		m_RayTracingDescriptorSet->Build();
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		std::vector<MeshAdresses> meshAddresses;
		std::vector<Vulture::Material> materials;
		auto modelView = scene.GetRegistry().view<Vulture::ModelComponent, Vulture::TransformComponent>();
		uint32_t meshSizes = 0;
		uint32_t materialSizes = 0;
		for (auto& entity : modelView)
		{
			auto& [modelComp, transformComp] = scene.GetRegistry().get<Vulture::ModelComponent, Vulture::TransformComponent>(entity);
			for (int i = 0; i < (int)modelComp.Model->GetMeshCount(); i++)
			{
				MeshAdresses adr{};
				adr.VertexAddress = modelComp.Model->GetMesh(i).GetVertexBuffer()->GetDeviceAddress();
				adr.IndexAddress = modelComp.Model->GetMesh(i).GetIndexBuffer()->GetDeviceAddress();

				Vulture::Material material = modelComp.Model->GetMaterial(i);

				materials.push_back(material);
				meshAddresses.push_back(adr);
			}
			meshSizes += sizeof(MeshAdresses) * modelComp.Model->GetMeshCount();
			materialSizes += sizeof(Vulture::Material) * modelComp.Model->GetMeshCount();
		}

		if (!meshSizes)
			VL_CORE_ASSERT(false, "No meshes found?");

		m_RayTracingDescriptorSet->GetBuffer(2)->WriteToBuffer(meshAddresses.data(), meshSizes, 0);
		m_RayTracingDescriptorSet->GetBuffer(3)->WriteToBuffer(materials.data(), materialSizes, 0);

		m_RayTracingDescriptorSet->GetBuffer(2)->Flush(meshSizes, 0);
		m_RayTracingDescriptorSet->GetBuffer(3)->Flush(materialSizes, 0);
	}

	CreateRayTracingPipeline();
	CreateShaderBindingTable();
}

void SceneRenderer::SetSkybox(Vulture::SkyboxComponent& skybox)
{
	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_GlobalDescriptorSets[i]->UpdateImageSampler(
			1,
			skybox.SkyboxImage->GetSamplerHandle(),
			skybox.SkyboxImage->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}
}

void SceneRenderer::RecreateDescriptorSets()
{
	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		
		m_HDRDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
		m_HDRDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin });
		m_HDRDescriptorSet->AddImageSampler(
			0,
			m_PresentedImage->GetSamplerHandle(),
			m_PresentedImage->GetImageView(),
			VK_IMAGE_LAYOUT_GENERAL
		);
		m_HDRDescriptorSet->Build();
	}
	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

		m_ToneMapDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
		m_ToneMapDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin });
		m_ToneMapDescriptorSet->AddImageSampler(
			0,
			m_PresentedImage->GetSamplerHandle(),
			m_PresentedImage->GetImageView(),
			VK_IMAGE_LAYOUT_GENERAL
		);
		m_ToneMapDescriptorSet->Build();
	}

	{
		m_RayTracingDescriptorSet->UpdateImageSampler(
			1,
			m_PathTracingImage->GetSamplerHandle(),
			m_PathTracingImage->GetImageView(),
			VK_IMAGE_LAYOUT_GENERAL
		);
	}

	CreateRayTracingDescriptorSets(*m_CurrentSceneRendered);
}

void SceneRenderer::CreatePipelines()
{
	{
		float matsize = sizeof(Vulture::Material);
		VkPushConstantRange range{};
		range.offset = 0;
		range.size = sizeof(PushConstantGBuffer);
		range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		// Configure pipeline creation parameters
		Vulture::Pipeline::CreateInfo info{};
		info.AttributeDesc = Vulture::Mesh::Vertex::GetAttributeDescriptions();
		info.BindingDesc = Vulture::Mesh::Vertex::GetBindingDescriptions();
		info.ShaderFilepaths.push_back("src/shaders/spv/GBuffer.vert.spv");
		info.ShaderFilepaths.push_back("src/shaders/spv/GBuffer.frag.spv");
		info.BlendingEnable = false;
		info.DepthTestEnable = true;
		info.CullMode = VK_CULL_MODE_NONE;
		info.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		info.Width = Vulture::Renderer::GetSwapchain().GetWidth();
		info.Height = Vulture::Renderer::GetSwapchain().GetHeight();
		info.PushConstants = &range;
		info.ColorAttachmentCount = 3;

		// Descriptor set layouts for the pipeline
		std::vector<VkDescriptorSetLayout> layouts
		{
			m_GlobalDescriptorSets[0]->GetDescriptorSetLayout()->GetDescriptorSetLayoutHandle()
		};
		info.DescriptorSetLayouts = layouts;

		m_GBufferPass.CreatePipeline(info);
	}
}

void SceneRenderer::CreateRayTracingPipeline()
{
	//
	// Ray Tracing
	//
	{
		VkPushConstantRange range{};
		range.offset = 0;
		range.size = sizeof(PushConstantRay);
		range.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;

		Vulture::Pipeline::RayTracingCreateInfo info{};
		info.PushConstants = &range;
		info.RayGenShaderFilepaths.push_back("src/shaders/spv/raytrace.rgen.spv");
		info.HitShaderFilepaths.push_back("src/shaders/spv/raytrace.rchit.spv");
		info.MissShaderFilepaths.push_back("src/shaders/spv/raytrace.rmiss.spv");

		// Descriptor set layouts for the pipeline
		std::vector<VkDescriptorSetLayout> layouts
		{
			m_RayTracingDescriptorSet->GetDescriptorSetLayout()->GetDescriptorSetLayoutHandle(),
			m_GlobalDescriptorSets[0]->GetDescriptorSetLayout()->GetDescriptorSetLayoutHandle()
		};
		info.DescriptorSetLayouts = layouts;

		m_RtPipeline.Init(&info);
	}
}

void SceneRenderer::CreateShaderBindingTable()
{
	Vulture::SBT::CreateInfo info{};
	info.CallableCount = 0;
	info.HitCount = 1;
	info.MissCount = 1;
	info.RGenCount = 1;
	info.RayTracingPipeline = &m_RtPipeline;

	m_SBT.Init(&info);
}

void SceneRenderer::CreateFramebuffers()
{
	// Path Tracing
	{
		Vulture::Image::CreateInfo info{};
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
		info.Height = Vulture::Renderer::GetSwapchain().GetHeight();
		info.Width = Vulture::Renderer::GetSwapchain().GetWidth();
		info.LayerCount = 1;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.SamplerInfo = Vulture::SamplerInfo{};
		info.Type = Vulture::Image::ImageType::Image2D;
		m_PathTracingImage = std::make_shared<Vulture::Image>(info);
		m_PathTracingImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);
	}

	// Denoised
	{
		Vulture::Image::CreateInfo info{};
		info.Width = Vulture::Renderer::GetSwapchain().GetWidth();
		info.Height = Vulture::Renderer::GetSwapchain().GetHeight();
		info.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.LayerCount = 1;
		info.SamplerInfo = Vulture::SamplerInfo{};
		info.Type = Vulture::Image::ImageType::Image2D;

		m_DenoisedImage = std::make_shared<Vulture::Image>(info);
		Vulture::Image::TransitionImageLayout(
			m_DenoisedImage->GetImage(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_TRANSFER_READ_BIT
		);
	}

	// TODO use this gbuffer to speed up path tracing and maybe do something with the formats?
	// GBuffer
	{
		std::vector<Vulture::FramebufferAttachment> attachments
		{ 
			Vulture::FramebufferAttachment::ColorRGBA32, // This has to be 32 per channel otherwise optix won't work
			Vulture::FramebufferAttachment::ColorRGBA32, // This has to be 32 per channel otherwise optix won't work
			Vulture::FramebufferAttachment::ColorRG8,
			Vulture::FramebufferAttachment::Depth
		};
		
		Vulture::Framebuffer::CreateInfo info{};
		info.AttachmentsFormats = &attachments;
		info.DepthFormat = Vulture::Swapchain::FindDepthFormat();
		info.Extent = Vulture::Renderer::GetSwapchain().GetSwapchainExtent();
		info.RenderPass = m_GBufferPass.GetRenderPass();
		info.CustomBits = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_GBufferFramebuffer = std::make_shared<Vulture::Framebuffer>(info);
	}
}

void SceneRenderer::UpdateDescriptorSetsData()
{
	auto cameraView = m_CurrentSceneRendered->GetRegistry().view<Vulture::CameraComponent>();
	Vulture::CameraComponent* camComp = nullptr;
	for (auto& entity : cameraView)
	{
		Vulture::CameraComponent& comp = m_CurrentSceneRendered->GetRegistry().get<Vulture::CameraComponent>(entity);
		if (comp.Main)
		{
			camComp = &comp;
			break;
		}
	}
	GlobalUbo ubo{};
	if (camComp == nullptr)
	{
		VL_CORE_ASSERT(false, "No main camera found!");
	}
	ubo.ProjInverse = glm::inverse(camComp->ProjMat);
	ubo.ViewInverse = glm::inverse(camComp->ViewMat);
	ubo.ViewProjectionMat = camComp->GetViewProj();
	m_GlobalDescriptorSets[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->WriteToBuffer(&ubo);

	m_GlobalDescriptorSets[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->Flush();
}

void SceneRenderer::ImGuiPass()
{
	static bool renderImGui = true;

	if (renderImGui == false && Vulture::Input::IsKeyPressed(VL_KEY_I))
	{
		renderImGui = true;
	}

	if (!renderImGui)
		return;

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");
	if(ImGui::Button("Hide"))
		renderImGui = false;

	ImGui::Text("ms %f | fps %f", m_Timer.ElapsedMillis(), 1.0f / m_Timer.ElapsedSeconds());
	ImGui::Text("Total vertices: %i", m_CurrentSceneRendered->GetVertexCount());
	ImGui::Text("Total indices: %i", m_CurrentSceneRendered->GetIndexCount());
	m_Timer.Reset();

	ImGui::Separator();

	Vulture::Entity cameraEntity;
	m_CurrentSceneRendered->GetMainCamera(&cameraEntity);
	ImGui::Text("Camera");

	Vulture::ScriptComponent* scComp;
	scComp = m_CurrentSceneRendered->GetRegistry().try_get<Vulture::ScriptComponent>(cameraEntity);
	if (scComp)
	{
		CameraScript* camScript = scComp->GetScript<CameraScript>(0);
		if (camScript)
		{
			ImGui::SliderFloat("Movement Speed", &camScript->m_MovementSpeed, 0.0f, 20.0f);
			ImGui::SliderFloat("Rotation Speed", &camScript->m_RotationSpeed, 0.0f, 4.0f);
			ImGui::Checkbox("Locked", &camScript->m_CameraLocked);
		}
	}
	ImGui::SliderFloat("Exposure", &m_Exposure, 0.0f, 3.0f);

	ImGui::Separator();
	ImGui::SliderInt("Max Depth", &m_MaxRayDepth, 0, 20);

	ImGui::Separator();
	ImGui::Text("Time: %fs", m_Time);
	ImGui::Text("Samples Per Pixel: %i", m_CurrentSamplesPerPixel);
	if (ImGui::Button("Reset"))
	{
		ResetFrame();
	}

	ImGui::Separator();

	if (ImGui::Checkbox("Show Denoised Image", &m_ShowDenoised))
	{
		if (m_ShowDenoised)
		{
			m_PresentedImage = m_DenoisedImage;
			{
				Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };

				m_HDRDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
				m_HDRDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin });
				m_HDRDescriptorSet->AddImageSampler(
					0,
					m_PresentedImage->GetSamplerHandle(),
					m_PresentedImage->GetImageView(),
					VK_IMAGE_LAYOUT_GENERAL
				);
				m_HDRDescriptorSet->Build();
			}
		}
		else
		{
			m_PresentedImage = m_PathTracingImage;
			{
				Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };

				m_HDRDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
				m_HDRDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin });
				m_HDRDescriptorSet->AddImageSampler(
					0,
					m_PresentedImage->GetSamplerHandle(),
					m_PresentedImage->GetImageView(),
					VK_IMAGE_LAYOUT_GENERAL
				);
				m_HDRDescriptorSet->Build();
			}
		}
	}

	if (ImGui::Button("Denoise"))
	{
		m_RunDenoising = true;
	}

	ImGui::SliderInt("Samples Per Pixel", &m_MaxSamplesPerPixel, 1, 50'000);

	ImGui::SliderFloat("Focal Length", &m_FocalLength, 1.0f, 100.0f);
	ImGui::SliderFloat("DoF Strength", &m_DoFStrength, 1.0f, 100.0f);

	ImGui::End();
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Vulture::Renderer::GetCurrentCommandBuffer());
}
