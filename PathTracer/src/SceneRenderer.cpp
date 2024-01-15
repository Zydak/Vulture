#include "pch.h"
#include "SceneRenderer.h"

#include "imgui.h"
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include "CameraScript.h"

SceneRenderer::SceneRenderer()
{
	Vulture::ImageInfo info{};
	info.width = Vulture::Renderer::GetSwapchain().GetWidth();
	info.height = Vulture::Renderer::GetSwapchain().GetHeight();
	info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	info.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	info.layerCount = 1;
	info.samplerInfo = Vulture::SamplerInfo{};
	info.type = Vulture::ImageType::Image2D;

	m_DenoisedImage = std::make_shared<Vulture::Image>(info);
	Vulture::Image::TransitionImageLayout(m_DenoisedImage->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	CreateRenderPasses();

	CreateFramebuffers();
	m_PresentedImageView = m_HDRFramebuffer->GetColorImageView(0);

	CreateUniforms();
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
		UpdateUniformData();
		RayTrace(glm::vec4(0.1f));

		// Run denoised if requested
		if (m_RunDenoising)
		{
			Denoise();
			m_RunDenoising = false;
		}

		Vulture::Renderer::FramebufferCopyPass(&(*m_HDRUniforms));

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
	if (previousMat != m_CurrentSceneRendered->GetMainCamera()->ViewMat)
	{
		ResetFrame();
		previousMat = m_CurrentSceneRendered->GetMainCamera()->ViewMat;
	}
	else
	{
		if (m_CurrentSamplesPerPixel >= (uint32_t)m_MaxSamplesPerPixel)
		{
			return;
		}

		m_PushContantRay.frame++;
		m_CurrentSamplesPerPixel += 15;
	}
	
	m_Time = m_TotalTimer.ElapsedSeconds();

	m_RtPipeline.Bind(Vulture::Renderer::GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	m_RayTracingUniforms->Bind(
		0,
		m_RtPipeline.GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);
	m_GlobalUniforms[Vulture::Renderer::GetCurrentFrameIndex()]->Bind(
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
		&m_RgenRegion, 
		&m_MissRegion, 
		&m_HitRegion, 
		&m_CallRegion,
		(uint32_t)m_HDRFramebuffer->GetExtent().x,
		(uint32_t)m_HDRFramebuffer->GetExtent().y,
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
	clearColors.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
	VkClearValue clearVal{};
	clearVal.depthStencil = { 1.0f, 1 };
	clearColors.push_back(clearVal);

	m_GBufferPass.SetRenderTarget(&(*m_GBufferFramebuffer));
	m_GBufferPass.BeginRenderPass(clearColors);

	m_GlobalUniforms[Vulture::Renderer::GetCurrentFrameIndex()]->Bind
	(
		0,
		m_GBufferPass.GetPipeline().GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);


	for (auto& entity : view)
	{
		auto& [modelComp, TransformComp] = m_CurrentSceneRendered->GetRegistry().get<Vulture::ModelComponent, Vulture::TransformComponent>(entity);
		
		std::vector<Vulture::Ref<Vulture::Mesh>> meshes = modelComp.Model.GetMeshes();
		for (uint32_t i = 0; i < modelComp.Model.GetMeshCount(); i++)
		{
			PushConstantGBuffer push;
			push.Material = modelComp.Model.GetMaterial(i);
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
		const_cast<Vulture::Image*>(&(m_HDRFramebuffer->GetColorImageNoVk(0))), // Path Tracing Result
		const_cast<Vulture::Image*>(&(m_GBufferFramebuffer->GetColorImageNoVk(GBufferImage::Albedo))), // Albedo
		const_cast<Vulture::Image*>(&(m_GBufferFramebuffer->GetColorImageNoVk(GBufferImage::Normal))) // Normal
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
}

void SceneRenderer::RecreateResources()
{
	ResetFrame();
	m_PushContantRay.frame -= 1;

	CreateFramebuffers();
	CreatePipelines();

	FixCameraAspectRatio();

	m_Denoiser->AllocateBuffers(Vulture::Renderer::GetSwapchain().GetSwapchainExtent());

	Vulture::ImageInfo info{};
	info.width = Vulture::Renderer::GetSwapchain().GetWidth();
	info.height = Vulture::Renderer::GetSwapchain().GetHeight();
	info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	info.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	info.layerCount = 1;
	info.samplerInfo = Vulture::SamplerInfo{};
	info.type = Vulture::ImageType::Image2D;

	m_DenoisedImage = std::make_shared<Vulture::Image>(info);
	Vulture::Image::TransitionImageLayout(m_DenoisedImage->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

	if (m_ShowDenoised)
	{
		m_PresentedImageView = m_DenoisedImage->GetImageView();
	}
	else
	{
		m_PresentedImageView = m_HDRFramebuffer->GetColorImageView(0);
	}

	RecreateUniforms();
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
		albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkAttachmentReference albedoAttachmentRef = {};
		albedoAttachmentRef.attachment = 0;
		albedoAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription roughnessAttachment = {};
		roughnessAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		roughnessAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		roughnessAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		roughnessAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		roughnessAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		roughnessAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		roughnessAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		roughnessAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkAttachmentReference roughnessAttachmentRef = {};
		roughnessAttachmentRef.attachment = 1;
		roughnessAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription metallnessAttachment = {};
		metallnessAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		metallnessAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		metallnessAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		metallnessAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		metallnessAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		metallnessAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		metallnessAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		metallnessAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkAttachmentReference metallnessAttachmentRef = {};
		metallnessAttachmentRef.attachment = 2;
		metallnessAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription normalAttachment = {};
		normalAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		normalAttachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkAttachmentReference normalAttachmentRef = {};
		normalAttachmentRef.attachment = 3;
		normalAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
		depthAttachmentRef.attachment = 4;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		std::vector<VkAttachmentReference> references { albedoAttachmentRef, roughnessAttachmentRef, metallnessAttachmentRef, normalAttachmentRef };

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 4;
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

		std::vector<VkAttachmentDescription> attachments { albedoAttachment, roughnessAttachment, metallnessAttachment, normalAttachment, depthAttachment };
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

void SceneRenderer::CreateUniforms()
{
	m_GlobalUniforms.clear();

	{
		m_HDRUniforms = std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool());
		m_HDRUniforms->AddImageSampler(0, Vulture::Renderer::GetSampler().GetSampler(), m_PresentedImageView,
			VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT
		);
		m_HDRUniforms->Build();
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_GlobalUniforms.push_back(std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool()));
		m_GlobalUniforms[i]->AddUniformBuffer(0, sizeof(GlobalUbo), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);

		m_GlobalUniforms[i]->Build();
	}
}

void SceneRenderer::CreateRayTracingUniforms(Vulture::Scene& scene)
{
	{
		m_RayTracingUniforms = std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool());

		VkAccelerationStructureKHR tlas = scene.GetAccelerationStructure()->GetTlas().Accel;
		VkWriteDescriptorSetAccelerationStructureKHR asInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		asInfo.accelerationStructureCount = 1;
		asInfo.pAccelerationStructures = &tlas;

		m_RayTracingUniforms->AddAccelerationStructure(0, asInfo);
		m_RayTracingUniforms->AddImageSampler(1, Vulture::Renderer::GetSampler().GetSampler(), m_HDRFramebuffer->GetColorImageView(0),
			VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		m_RayTracingUniforms->AddStorageBuffer(2, sizeof(MeshAdresses) * 100, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, true);
		m_RayTracingUniforms->AddStorageBuffer(3, sizeof(Vulture::Material) * 100, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, true);

		m_RayTracingUniforms->Build();
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
			for (int i = 0; i < (int)modelComp.Model.GetMeshCount(); i++)
			{
				MeshAdresses adr{};
				adr.VertexAddress = modelComp.Model.GetMesh(i).GetVertexBuffer()->GetDeviceAddress();
				adr.IndexAddress = modelComp.Model.GetMesh(i).GetIndexBuffer()->GetDeviceAddress();

				Vulture::Material material = modelComp.Model.GetMaterial(i);

				materials.push_back(material);
				meshAddresses.push_back(adr);
			}
			meshSizes += sizeof(MeshAdresses) * modelComp.Model.GetMeshCount();
			materialSizes += sizeof(Vulture::Material) * modelComp.Model.GetMeshCount();
		}

		if (!meshSizes)
			VL_CORE_ASSERT(false, "No meshes found?");

		m_RayTracingUniforms->GetBuffer(2)->WriteToBuffer(meshAddresses.data(), meshSizes, 0);
		m_RayTracingUniforms->GetBuffer(3)->WriteToBuffer(materials.data(), materialSizes, 0);

		m_RayTracingUniforms->GetBuffer(2)->Flush(meshSizes, 0);
		m_RayTracingUniforms->GetBuffer(3)->Flush(materialSizes, 0);
	}

	CreateRayTracingPipeline();
	CreateShaderBindingTable();
}

void SceneRenderer::RecreateUniforms()
{
	{
		m_HDRUniforms = std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool());
		m_HDRUniforms->AddImageSampler(0, Vulture::Renderer::GetSampler().GetSampler(), m_PresentedImageView,
			VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT
		);
		m_HDRUniforms->Build();
	}

	{
		m_RayTracingUniforms->UpdateImageSampler(1, m_HDRFramebuffer->GetColorImageView(0), Vulture::Renderer::GetSampler().GetSampler(),
			VK_IMAGE_LAYOUT_GENERAL);
	}

	CreateRayTracingUniforms(*m_CurrentSceneRendered);
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
		Vulture::PipelineCreateInfo info{};
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
		info.ColorAttachmentCount = 4;

		// Descriptor set layouts for the pipeline
		std::vector<VkDescriptorSetLayout> layouts
		{
			m_GlobalUniforms[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout()
		};
		info.UniformSetLayouts = layouts;

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

		Vulture::RayTracingPipelineCreateInfo info{};
		info.PushConstants = &range;
		// TODO
		//info.ShaderFilepaths = something;

		// Descriptor set layouts for the pipeline
		std::vector<VkDescriptorSetLayout> layouts
		{
			m_RayTracingUniforms->GetDescriptorSetLayout()->GetDescriptorSetLayout(),
			m_GlobalUniforms[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout()
		};
		info.UniformSetLayouts = layouts;

		m_RtPipeline.CreateRayTracingPipeline(m_RtShaderGroups, info);
	}
}

void SceneRenderer::CreateShaderBindingTable()
{
	uint32_t missCount = 1;
	uint32_t hitCount = 1;
	uint32_t handleCount = 1 + missCount + hitCount;
	uint32_t handleSize = Vulture::Device::GetRayTracingProperties().shaderGroupHandleSize;

	uint32_t handleSizeAligned = (uint32_t)Vulture::Device::GetAlignment(handleSize, Vulture::Device::GetRayTracingProperties().shaderGroupHandleAlignment);

	m_RgenRegion.stride = Vulture::Device::GetAlignment(handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);
	m_RgenRegion.size = m_RgenRegion.stride;
	
	m_MissRegion.stride = handleSizeAligned;
	m_MissRegion.size = Vulture::Device::GetAlignment(missCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);
	
	m_HitRegion.stride = handleSizeAligned;
	m_HitRegion.size = Vulture::Device::GetAlignment(hitCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);

	// Get the shader group handles
	uint32_t             dataSize = handleCount * handleSize;
	std::vector<uint8_t> handles(dataSize);
	auto result = Vulture::Device::vkGetRayTracingShaderGroupHandlesKHR(Vulture::Device::GetDevice(), m_RtPipeline.GetPipeline(), 0, handleCount, dataSize, handles.data());
	VL_CORE_ASSERT(result == VK_SUCCESS, "Failed to get shader group handles!");

	// Allocate a buffer for storing the SBT.
	VkDeviceSize sbtSize = m_RgenRegion.size + m_MissRegion.size + m_HitRegion.size + m_CallRegion.size;
	Vulture::Ref<Vulture::Buffer> StagingBuffer = std::make_shared<Vulture::Buffer>(
		sbtSize,
		1,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	m_RtSBTBuffer = std::make_shared<Vulture::Buffer>(
		sbtSize,
		1,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);

	// Find the SBT addresses of each group
	m_RgenRegion.deviceAddress = m_RtSBTBuffer->GetDeviceAddress();
	m_MissRegion.deviceAddress = m_RtSBTBuffer->GetDeviceAddress() + m_RgenRegion.size;
	m_HitRegion.deviceAddress = m_RtSBTBuffer->GetDeviceAddress() + m_RgenRegion.size + m_MissRegion.size;

	auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

	// TODO maybe use WriteToBuffer?
	StagingBuffer->Map();
	uint8_t* pSBTBuffer = reinterpret_cast<uint8_t*>(StagingBuffer->GetMappedMemory());
	uint8_t* pData = nullptr;
	uint32_t handleIdx = 0;

	// Ray gen
	pData = pSBTBuffer;
	memcpy(pData, getHandle(handleIdx), handleSize);
	handleIdx++;

	// Miss
	pData = pSBTBuffer + m_RgenRegion.size;
	for (uint32_t c = 0; c < missCount; c++)
	{
		memcpy(pData, getHandle(handleIdx), handleSize);
		handleIdx++;
		pData += m_MissRegion.stride;
	}

	// Hit
	pData = pSBTBuffer + m_RgenRegion.size + m_MissRegion.size;
	for (uint32_t c = 0; c < hitCount; c++)
	{
		memcpy(pData, getHandle(handleIdx), handleSize);
		handleIdx++;
		pData += m_HitRegion.stride;
	}

	// Copy the shader binding table to the device local buffer
	Vulture::Buffer::CopyBuffer(StagingBuffer->GetBuffer(), m_RtSBTBuffer->GetBuffer(), sbtSize, Vulture::Device::GetGraphicsQueue(), Vulture::Device::GetCommandPool());
	
	StagingBuffer->Unmap();
}

void SceneRenderer::CreateFramebuffers()
{
	// HDR
	{
		std::vector<Vulture::FramebufferAttachment> attachments{ Vulture::FramebufferAttachment::ColorRGBA32 };
		{
			m_HDRFramebuffer = std::make_shared<Vulture::Framebuffer>(attachments, m_HDRPass.GetRenderPass(), Vulture::Renderer::GetSwapchain().GetSwapchainExtent(), Vulture::Swapchain::FindDepthFormat(), 1, Vulture::ImageType::Image2D, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
			Vulture::Image::TransitionImageLayout(m_HDRFramebuffer->GetColorImage(0), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		}
	}

	// GBuffer
	{
		std::vector<Vulture::FramebufferAttachment> attachments{ Vulture::FramebufferAttachment::ColorRGBA32, Vulture::FramebufferAttachment::ColorRGBA32, Vulture::FramebufferAttachment::ColorRGBA32, Vulture::FramebufferAttachment::ColorRGBA32, Vulture::FramebufferAttachment::Depth };
		
		m_GBufferFramebuffer = std::make_shared<Vulture::Framebuffer>(attachments, m_GBufferPass.GetRenderPass(), Vulture::Renderer::GetSwapchain().GetSwapchainExtent(), Vulture::Swapchain::FindDepthFormat(), 1, Vulture::ImageType::Image2D, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		Vulture::Image::TransitionImageLayout(m_GBufferFramebuffer->GetColorImage(GBufferImage::Albedo), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		Vulture::Image::TransitionImageLayout(m_GBufferFramebuffer->GetColorImage(GBufferImage::Roughness), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		Vulture::Image::TransitionImageLayout(m_GBufferFramebuffer->GetColorImage(GBufferImage::Metallness), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		Vulture::Image::TransitionImageLayout(m_GBufferFramebuffer->GetColorImage(GBufferImage::Normal), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	}
}

void SceneRenderer::UpdateUniformData()
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
	m_GlobalUniforms[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->WriteToBuffer(&ubo);

	m_GlobalUniforms[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->Flush();
}

void SceneRenderer::ImGuiPass()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");

	ImGui::Text("ms %f | fps %f", m_Timer.ElapsedMillis(), 1.0f / m_Timer.ElapsedSeconds());
	m_Timer.Reset();

	ImGui::Separator();
	auto cameraView = m_CurrentSceneRendered->GetRegistry().view<Vulture::CameraComponent>();
	for (auto& entity : cameraView)
	{
		Vulture::CameraComponent& comp = m_CurrentSceneRendered->GetRegistry().get<Vulture::CameraComponent>(entity);
		if (comp.Main)
		{
			ImGui::Text("Camera");
			
			Vulture::ScriptComponent* scComp;
			scComp = m_CurrentSceneRendered->GetRegistry().try_get<Vulture::ScriptComponent>(entity);
			if (scComp)
			{
				CameraScript* camScript = scComp->GetScript<CameraScript>(0);
				if (camScript)
				{
					if (ImGui::Checkbox("Orbit Camera", &camScript->m_OrbitCamera) && camScript->m_OrbitCamera == false)
					{
						comp.Translation = glm::vec3(0.0f, 0.0f, -10.0f);
					}

					ImGui::SliderFloat("Movement Speed", &camScript->m_MovementSpeed, 0.0f, 20.0f);
					ImGui::SliderFloat("Rotation Speed", &camScript->m_RotationSpeed, 0.0f, 4.0f);
					ImGui::Checkbox("Locked", &camScript->m_CameraLocked);
				}
			}

			break;
		}
	}

	ImGui::Separator();
	ImGui::SliderInt("Max Depth", &m_MaxRayDepth, 0, 20);

	ImGui::Separator();
	ImGui::Text("Samples Per Pixel: %i", m_CurrentSamplesPerPixel);
	ImGui::Text("Time: %fs", m_Time);

	if (ImGui::Checkbox("Show Denoised Image", &m_ShowDenoised))
	{
		if (m_ShowDenoised)
		{
			m_HDRUniforms = std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool());
			m_HDRUniforms->AddImageSampler(0, Vulture::Renderer::GetSampler().GetSampler(), m_DenoisedImage->GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT
			);
			m_HDRUniforms->Build();
			m_PresentedImageView = m_DenoisedImage->GetImageView();
		}
		else
		{
			m_HDRUniforms = std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool());
			m_HDRUniforms->AddImageSampler(0, Vulture::Renderer::GetSampler().GetSampler(), m_HDRFramebuffer->GetColorImageView(0),
				VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT
			);
			m_HDRUniforms->Build();
			m_PresentedImageView = m_HDRFramebuffer->GetColorImageView(0);
		}
	}

	ImGui::SliderInt("Samples Per Pixel", &m_MaxSamplesPerPixel, 1, 50'000);

	if (ImGui::Button("Denoise"))
	{
		m_RunDenoising = true;
	}

	if (ImGui::Button("Reset"))
	{
		ResetFrame();
	}

	ImGui::SliderFloat("Focal Length", &m_FocalLength, 1.0f, 100.0f);
	ImGui::SliderFloat("DoF Strength", &m_DoFStrength, 1.0f, 100.0f);

	ImGui::End();
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Vulture::Renderer::GetCurrentCommandBuffer());
}
