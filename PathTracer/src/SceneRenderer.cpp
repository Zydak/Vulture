#include "pch.h"
#include "SceneRenderer.h"

#include "imgui.h"
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include "CameraScript.h"

SceneRenderer::SceneRenderer()
{
	//Vulture::Ref<Vulture::Image> tempImage = std::make_shared<Vulture::Image>("../Vulture/assets/black.hdr");
	//
	Vulture::Image::CreateInfo imageInfo = {};
	imageInfo.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	imageInfo.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
	imageInfo.Height = 1024;
	imageInfo.Width = 1024;
	imageInfo.LayerCount = 6;
	imageInfo.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	imageInfo.SamplerInfo = {};
	imageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.Type = Vulture::Image::ImageType::Image2D;
	imageInfo.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	m_Skybox = std::make_shared<Vulture::Image>(imageInfo);
	//
	//Vulture::Renderer::EnvMapToCubemapPass(tempImage, m_Skybox, 1);

	CreateRenderPasses();

	CreateFramebuffers();
	m_PresentedImage = m_PathTracingImage;

	CreateDescriptorSets();
	CreatePipelines();

	m_Denoiser = std::make_shared<Vulture::Denoiser>();
	m_Denoiser->Init();
	m_Denoiser->AllocateBuffers(m_ViewportSize);

	VkFenceCreateInfo createInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	vkCreateFence(Vulture::Device::GetDevice(), &createInfo, nullptr, &m_DenoiseFence);

	Vulture::Renderer::RenderImGui([this](){ImGuiPass(); });
	m_TotalTimer.Reset();
}

SceneRenderer::~SceneRenderer()
{
	vkDestroyFence(Vulture::Device::GetDevice(), m_DenoiseFence, nullptr);
	m_Denoiser->Destroy();
}

void SceneRenderer::Render(Vulture::Scene& scene)
{
	m_CurrentSceneRendered = &scene;

	if (m_DrawIntoAFileChanged)
	{
		RecreateResources();
		m_DrawIntoAFileChanged = false;

		if (m_DrawIntoAFile)
		{
			m_PresentedImage = m_PathTracingImage;
			CreateHDRSet();
		}
	}

	if (m_DrawIntoAFile)
	{
		if (Vulture::Renderer::BeginFrame())
		{
			static bool finished = false;
			UpdateDescriptorSetsData();
			bool rayTracingFinished = !RayTrace(glm::vec4(0.1f));
			if (rayTracingFinished)
			{
				m_DrawFileInfo.DrawingFramebufferFinished = true;
				
				// tone map and denoise if finished
				if (!m_ToneMapped)
				{
					m_PathTracingImage->TransitionImageLayout(
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						Vulture::Renderer::GetCurrentCommandBuffer()
					);

					m_BloomImage->TransitionImageLayout(
						VK_IMAGE_LAYOUT_GENERAL,
						0,
						VK_ACCESS_SHADER_WRITE_BIT,
						0,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						Vulture::Renderer::GetCurrentCommandBuffer()
					);

					Vulture::Renderer::BloomPass(m_PathTracingImage, m_BloomImage, m_DrawInfo.BloomInfo);

					m_PathTracingImage->TransitionImageLayout(
						VK_IMAGE_LAYOUT_GENERAL,
						VK_ACCESS_SHADER_READ_BIT,
						VK_ACCESS_SHADER_WRITE_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						Vulture::Renderer::GetCurrentCommandBuffer()
					);

					m_BloomImage->TransitionImageLayout(
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_ACCESS_SHADER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						Vulture::Renderer::GetCurrentCommandBuffer()
					);

					m_TonemappedImage->TransitionImageLayout(
						VK_IMAGE_LAYOUT_GENERAL,
						0,
						VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
						0,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
						Vulture::Renderer::GetCurrentCommandBuffer()
					);

					Vulture::Renderer::ToneMapPass(m_ToneMapDescriptorSet, m_TonemappedImage->GetImageSize(), m_DrawInfo.TonemapInfo);

					m_TonemappedImage->TransitionImageLayout(
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
						VK_ACCESS_SHADER_WRITE_BIT,
						VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT,
						VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
						VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
						Vulture::Renderer::GetCurrentCommandBuffer()
					);
					m_ToneMapped = true;

					Denoise();
					m_PresentedImage = m_DenoisedImage;

					CreateHDRSet();
					finished = true;
				}
			}
			else
			{
				m_PathTracingImage->TransitionImageLayout(
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
					VK_ACCESS_SHADER_READ_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					Vulture::Renderer::GetCurrentCommandBuffer()
				);
			}

			Vulture::Renderer::ImGuiPass();

			if (!rayTracingFinished)
			{
				m_PathTracingImage->TransitionImageLayout(
					VK_IMAGE_LAYOUT_GENERAL,
					VK_ACCESS_SHADER_READ_BIT,
					0,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					0,
					Vulture::Renderer::GetCurrentCommandBuffer()
				);
			}

			Vulture::Renderer::EndFrame();

			if (finished)
			{
				vkDeviceWaitIdle(Vulture::Device::GetDevice());
				VkCommandBuffer snCmd;
				Vulture::Device::BeginSingleTimeCommands(snCmd, Vulture::Device::GetGraphicsCommandPool());
				m_PresentedImage->TransitionImageLayout(
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					VK_ACCESS_SHADER_WRITE_BIT,
					VK_ACCESS_TRANSFER_READ_BIT,
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					snCmd
				);
				Vulture::Renderer::SaveImageToFile("", m_PresentedImage, snCmd);

				m_PresentedImage->TransitionImageLayout(
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					0,
					VK_ACCESS_SHADER_READ_BIT,
					0,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
				);
				finished = false;
			}
		}
	}
	else if (m_ImGuiViewportResized)
	{
		RecreateResources();
		m_ImGuiViewportResized = false;
	}
	else if (Vulture::Renderer::BeginFrame())
	{
		UpdateDescriptorSetsData();
		RayTrace(glm::vec4(0.1f));

		m_BloomImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			0,
			VK_ACCESS_SHADER_WRITE_BIT,
			0,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		Vulture::Renderer::BloomPass(m_PathTracingImage, m_BloomImage, m_DrawInfo.BloomInfo);

		m_PathTracingImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_TRANSFER_READ_BIT,
			0,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		// prev layout is GENERAL
// 		m_PathTracingImage->TransitionImageLayout(
// 			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // new layout
// 			VK_ACCESS_SHADER_WRITE_BIT, // src access
// 			VK_ACCESS_TRANSFER_READ_BIT, // dst access
// 			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, // src stage
// 			VK_PIPELINE_STAGE_TRANSFER_BIT, // dst stage
// 			Vulture::Renderer::GetCurrentCommandBuffer()
// 		);
// 
// 		// prev layout is SHADER_READ_ONLY_OPTIMAL
// 		m_BloomImage->TransitionImageLayout(
// 			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // new layout
// 			VK_ACCESS_SHADER_READ_BIT, // src access
// 			VK_ACCESS_TRANSFER_WRITE_BIT, // dst access
// 			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // src stage
// 			VK_PIPELINE_STAGE_TRANSFER_BIT, // dst stage
// 			Vulture::Renderer::GetCurrentCommandBuffer()
// 		);
// 
// 		// prev layout is TRANSFER_SRC_OPTIMAL
// 		m_PathTracingImage->TransitionImageLayout(
// 			VK_IMAGE_LAYOUT_GENERAL, // new layout
// 			VK_ACCESS_TRANSFER_READ_BIT, // src access
// 			VK_ACCESS_SHADER_WRITE_BIT, // dst access
// 			VK_PIPELINE_STAGE_TRANSFER_BIT, // src stage
// 			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // dst stage
// 			Vulture::Renderer::GetCurrentCommandBuffer()
// 		);
// 
// 		// prev layout is LAYOUT_TRANSFER_DST_OPTIMAL
// 		m_BloomImage->TransitionImageLayout(
// 			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // new layout
// 			VK_ACCESS_TRANSFER_WRITE_BIT, // src access
// 			VK_ACCESS_SHADER_READ_BIT, // dst access
// 			VK_PIPELINE_STAGE_TRANSFER_BIT, // src stage
// 			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // dst stage
// 			Vulture::Renderer::GetCurrentCommandBuffer()
// 		);
// 
		m_TonemappedImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			0,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
			0,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		m_BloomImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		Vulture::Renderer::ToneMapPass(m_ToneMapDescriptorSet, m_TonemappedImage->GetImageSize(), m_DrawInfo.TonemapInfo);

		m_BloomImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			VK_ACCESS_SHADER_READ_BIT,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		m_TonemappedImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		// Run denoised if requested
		if (m_RunDenoising)
		{
			Denoise();
			m_RunDenoising = false;
			m_Denoised = true;
		}

		//Vulture::Renderer::FramebufferCopyPassImGui(m_HDRDescriptorSet);
		Vulture::Renderer::ImGuiPass();
		
		if (!Vulture::Renderer::EndFrame())
			RecreateResources();
	}
	else
	{
		RecreateResources();
	}
}

// TODO descritpion
bool SceneRenderer::RayTrace(const glm::vec4& clearColor)
{
	m_PushContantRayTrace.GetDataPtr()->ClearColor = clearColor;
	m_PushContantRayTrace.GetDataPtr()->maxDepth = m_DrawInfo.RayDepth;
	m_PushContantRayTrace.GetDataPtr()->FocalLength = m_DrawInfo.FocalLength;
	m_PushContantRayTrace.GetDataPtr()->DoFStrength = m_DrawInfo.DOFStrength;
	m_PushContantRayTrace.GetDataPtr()->SamplesPerFrame = m_DrawInfo.SamplesPerFrame;
	m_PushContantRayTrace.GetDataPtr()->EnvAzimuth =  glm::radians(m_DrawInfo.EnvAzimuth);
	m_PushContantRayTrace.GetDataPtr()->EnvAltitude = glm::radians(m_DrawInfo.EnvAltitude);
	m_PushContantRayTrace.GetDataPtr()->SamplesPerFrame = m_DrawInfo.SamplesPerFrame;

	// Draw Albedo, Roughness, Metallness, Normal into GBuffer
	DrawGBuffer();

	static glm::mat4 previousMat{ 0.0f };
	if (previousMat != m_CurrentSceneRendered->GetMainCamera()->ViewMat) // if camera moved
	{
		ResetFrame();
		previousMat = m_CurrentSceneRendered->GetMainCamera()->ViewMat;
	}
	else
	{
		if (m_CurrentSamplesPerPixel >= (uint32_t)m_DrawInfo.TotalSamplesPerPixel)
		{
			return false;
		}

		m_PushContantRayTrace.GetDataPtr()->frame++;
		m_CurrentSamplesPerPixel += m_DrawInfo.SamplesPerFrame;
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

	m_PushContantRayTrace.Push(m_RtPipeline.GetPipelineLayout(), Vulture::Renderer::GetCurrentCommandBuffer());

	Vulture::Renderer::RayTrace(Vulture::Renderer::GetCurrentCommandBuffer(), &m_SBT, m_PathTracingImage->GetImageSize());

	return true;
}

void SceneRenderer::DrawGBuffer()
{
	if (!m_DrawGBuffer)
		return;

	m_DrawGBuffer = false;
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
	// TODO
	m_GBufferFramebuffer->GetColorImageNoVk(0)->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_GBufferFramebuffer->GetColorImageNoVk(1)->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_GBufferFramebuffer->GetColorImageNoVk(2)->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_GBufferFramebuffer->GetColorImageNoVk(3)->SetLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_GBufferFramebuffer->GetDepthImageNoVk(0)->SetLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

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
			m_PushContantGBuffer.GetDataPtr()->Material = modelComp.Model->GetMaterial(i);
			m_PushContantGBuffer.GetDataPtr()->Model = TransformComp.transform.GetMat4();
			
			m_PushContantGBuffer.Push(m_GBufferPass.GetPipeline().GetPipelineLayout(), Vulture::Renderer::GetCurrentCommandBuffer());

			meshes[i]->Bind(Vulture::Renderer::GetCurrentCommandBuffer());
			meshes[i]->Draw(Vulture::Renderer::GetCurrentCommandBuffer(), 1, 0);
		}
	}

	m_GBufferPass.EndRenderPass();
}

void SceneRenderer::Denoise()
{
	VkCommandBuffer cmd = Vulture::Renderer::GetCurrentCommandBuffer();

	// copy images to cuda buffers
	std::vector<Vulture::Image*> vec = 
	{
		&(*m_TonemappedImage), // Path Tracing Result
		const_cast<Vulture::Image*>((m_GBufferFramebuffer->GetColorImageNoVk(GBufferImage::Albedo))), // Albedo
		const_cast<Vulture::Image*>((m_GBufferFramebuffer->GetColorImageNoVk(GBufferImage::Normal))) // Normal
	};

	vkEndCommandBuffer(cmd);

	// Increment for signaling
	m_DenoiseFenceValue++;

	VkSubmitInfo submits{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submits.commandBufferCount = 1;
	submits.pCommandBuffers = &cmd;

	submits.signalSemaphoreCount = 1;
	VkSemaphore sem = m_Denoiser->GetTLSemaphore();
	submits.pSignalSemaphores = &sem;

	VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{};
	timelineSubmitInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
	timelineSubmitInfo.pSignalSemaphoreValues = &m_DenoiseFenceValue;
	timelineSubmitInfo.signalSemaphoreValueCount = 1;
	timelineSubmitInfo.waitSemaphoreValueCount = 0;

	submits.pNext = &timelineSubmitInfo;

	// Submit command buffer so that resources can be used by Optix and CUDA
	vkResetFences(Vulture::Device::GetDevice(), 1, &m_DenoiseFence);
	vkQueueSubmit(Vulture::Device::GetGraphicsQueue(), 1, &submits, m_DenoiseFence);

	VkResult res = vkWaitForFences(Vulture::Device::GetDevice(), 1, &m_DenoiseFence, VK_TRUE, UINT64_MAX);

	// TODO: don't stall here
	VkCommandBuffer singleCmd;
	Vulture::Device::BeginSingleTimeCommands(singleCmd, Vulture::Device::GetGraphicsCommandPool());
	m_Denoiser->ImageToBuffer(singleCmd, vec);
	Vulture::Device::EndSingleTimeCommands(singleCmd, Vulture::Device::GetGraphicsQueue(), Vulture::Device::GetGraphicsCommandPool());

	// Run Denoiser
	m_Denoiser->DenoiseImageBuffer(m_DenoiseFenceValue, 0.0f);

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
	m_PushContantRayTrace.GetDataPtr()->frame = -1;
	m_CurrentSamplesPerPixel = 0;
	m_TotalTimer.Reset();
	m_ToneMapped = false;

	m_DrawGBuffer = true;
}

void SceneRenderer::RecreateResources()
{
	vkDeviceWaitIdle(Vulture::Device::GetDevice());
	ResetFrame();
	m_PushContantRayTrace.GetDataPtr()->frame -= 1;

	CreateFramebuffers();
	CreatePipelines();

	FixCameraAspectRatio();

	m_Denoiser->AllocateBuffers(m_ViewportSize);

	if (m_ShowDenoised)
	{
		m_PresentedImage = m_DenoisedImage;
	}
	else
	{
		m_PresentedImage = m_TonemappedImage;
	}

	RecreateDescriptorSets();
}

void SceneRenderer::FixCameraAspectRatio()
{
	float newAspectRatio = (float)m_ViewportSize.width / (float)m_ViewportSize.height;
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

		VkAttachmentDescription emissiveAttachment = {};
		emissiveAttachment.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		emissiveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		emissiveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		emissiveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		emissiveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		emissiveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		emissiveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		emissiveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference emissiveAttachmentRef = {};
		emissiveAttachmentRef.attachment = 3;
		emissiveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = Vulture::Swapchain::FindDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 4;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		std::vector<VkAttachmentReference> references { albedoAttachmentRef, normalAttachmentRef, RoghnessMetallnessAttachmentRef, emissiveAttachmentRef };

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

		std::vector<VkAttachmentDescription> attachments { albedoAttachment, normalAttachment, rougnessMetallnessAttachment, emissiveAttachment, depthAttachment };
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

	CreateHDRSet();

	// Tonemap
	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
		Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

		m_ToneMapDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
		m_ToneMapDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
		m_ToneMapDescriptorSet->AddImageSampler(
			0,
			m_BloomImage->GetSamplerHandle(),
			m_BloomImage->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
		m_ToneMapDescriptorSet->AddImageSampler(
			1,
			m_TonemappedImage->GetSamplerHandle(),
			m_TonemappedImage->GetImageView(),
			VK_IMAGE_LAYOUT_GENERAL
		);
		m_ToneMapDescriptorSet->Build();
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin2{ 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

		m_GlobalDescriptorSets.push_back(std::make_shared<Vulture::DescriptorSet>());
		m_GlobalDescriptorSets[i]->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1, bin2 });
		m_GlobalDescriptorSets[i]->AddUniformBuffer(0, sizeof(GlobalUbo));

		m_GlobalDescriptorSets[i]->AddImageSampler(
			1,
			Vulture::Renderer::GetEnv()->GetSamplerHandle(),
			Vulture::Renderer::GetEnv()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		m_GlobalDescriptorSets[i]->AddStorageBuffer(2, (uint32_t)Vulture::Renderer::GetEnv()->GetAccelBuffer()->GetBufferSize(), false, true);

		m_GlobalDescriptorSets[i]->Build();

		Vulture::Buffer::CopyBuffer(
			Vulture::Renderer::GetEnv()->GetAccelBuffer()->GetBuffer(),
			m_GlobalDescriptorSets[i]->GetBuffer(2)->GetBuffer(),
			Vulture::Renderer::GetEnv()->GetAccelBuffer()->GetBufferSize(),
			Vulture::Device::GetGraphicsQueue(),
			0,
			Vulture::Device::GetGraphicsCommandPool()
		);
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
		m_RayTracingDescriptorSet->AddStorageBuffer(2, sizeof(MeshAdresses) * 200, false, true);
		m_RayTracingDescriptorSet->AddStorageBuffer(3, sizeof(Vulture::Material) * 200, false, true);

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
	m_Denoised = false;
	m_PresentedImage = m_TonemappedImage;
	m_ShowDenoised = false;
	{
		CreateHDRSet();
	}
	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
		Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

		m_ToneMapDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
		m_ToneMapDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
		m_ToneMapDescriptorSet->AddImageSampler(
			0,
			m_BloomImage->GetSamplerHandle(),
			m_BloomImage->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
		m_ToneMapDescriptorSet->AddImageSampler(
			1,
			m_TonemappedImage->GetSamplerHandle(),
			m_TonemappedImage->GetImageView(),
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
		Vulture::PushConstant<PushConstantGBuffer>::CreateInfo pushInfo{};
		pushInfo.Stage = VK_SHADER_STAGE_VERTEX_BIT;

		m_PushContantGBuffer.Init(pushInfo);

		// Configure pipeline creation parameters
		Vulture::Pipeline::GraphicsCreateInfo info{};
		info.AttributeDesc = Vulture::Mesh::Vertex::GetAttributeDescriptions();
		info.BindingDesc = Vulture::Mesh::Vertex::GetBindingDescriptions();
		info.ShaderFilepaths.push_back("src/shaders/spv/GBuffer.vert.spv");
		info.ShaderFilepaths.push_back("src/shaders/spv/GBuffer.frag.spv");
		info.BlendingEnable = false;
		info.DepthTestEnable = true;
		info.CullMode = VK_CULL_MODE_NONE;
		info.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		info.Width = m_ViewportSize.width;
		info.Height = m_ViewportSize.height;
		info.PushConstants = &m_PushContantGBuffer.GetRange();
		info.ColorAttachmentCount = 4;

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
		Vulture::PushConstant<PushConstantRay>::CreateInfo pushInfo{};
		pushInfo.Stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
		
		m_PushContantRayTrace.Init(pushInfo);

		Vulture::Pipeline::RayTracingCreateInfo info{};
		info.PushConstants = &m_PushContantRayTrace.GetRange();
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

		m_RtPipeline.Init(info);
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
		info.Height = m_ViewportSize.height;
		info.Width = m_ViewportSize.width;
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
		info.Width = m_ViewportSize.width;
		info.Height = m_ViewportSize.height;
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
			Vulture::FramebufferAttachment::ColorRGBA32, // This has to be 32 per channel otherwise optix won't work
			Vulture::FramebufferAttachment::Depth
		};
		
		Vulture::Framebuffer::CreateInfo info{};
		info.AttachmentsFormats = &attachments;
		info.DepthFormat = Vulture::Swapchain::FindDepthFormat();
		info.Extent = m_ViewportSize;
		info.RenderPass = m_GBufferPass.GetRenderPass();
		info.CustomBits = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		m_GBufferFramebuffer = std::make_shared<Vulture::Framebuffer>(info);

#ifdef VL_IMGUI
		m_ImGuiAlbedoDescriptor = ImGui_ImplVulkan_AddTexture(m_GBufferFramebuffer->GetColorImageNoVk(0)->GetSamplerHandle(), m_GBufferFramebuffer->GetColorImageView(0), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		m_ImGuiNormalDescriptor = ImGui_ImplVulkan_AddTexture(m_GBufferFramebuffer->GetColorImageNoVk(1)->GetSamplerHandle(), m_GBufferFramebuffer->GetColorImageView(1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		m_ImGuiRoughnessDescriptor = ImGui_ImplVulkan_AddTexture(m_GBufferFramebuffer->GetColorImageNoVk(2)->GetSamplerHandle(), m_GBufferFramebuffer->GetColorImageView(2), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		m_ImGuiEmissiveDescriptor = ImGui_ImplVulkan_AddTexture(m_GBufferFramebuffer->GetColorImageNoVk(3)->GetSamplerHandle(), m_GBufferFramebuffer->GetColorImageView(3), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
	}

	// Tonemapped
	{
		Vulture::Image::CreateInfo info{};
		info.Width = m_ViewportSize.width;
		info.Height = m_ViewportSize.height;
		info.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.LayerCount = 1;
		info.SamplerInfo = Vulture::SamplerInfo{};
		info.Type = Vulture::Image::ImageType::Image2D;

		m_TonemappedImage = std::make_shared<Vulture::Image>();
		m_TonemappedImage->Init(info);
	}

	// Bloom
	{
		Vulture::Image::CreateInfo info{};
		info.Width = m_ViewportSize.width;
		info.Height = m_ViewportSize.height;
		info.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // TODO: check all usages in general, especially here, the transfer one
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.LayerCount = 1;
		info.SamplerInfo = Vulture::SamplerInfo{};
		info.Type = Vulture::Image::ImageType::Image2D;

		m_BloomImage = std::make_shared<Vulture::Image>();
		m_BloomImage->Init(info);
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

void SceneRenderer::CreateHDRSet()
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

#ifdef VL_IMGUI
	m_ImGuiViewportDescriptor = ImGui_ImplVulkan_AddTexture(m_PresentedImage->GetSamplerHandle(), m_PresentedImage->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
#endif
}

void SceneRenderer::ImGuiPass()
{
	Vulture::Entity cameraEntity;
	m_CurrentSceneRendered->GetMainCamera(&cameraEntity);

	Vulture::ScriptComponent* scComp = m_CurrentSceneRendered->GetRegistry().try_get<Vulture::ScriptComponent>(cameraEntity);
	Vulture::CameraComponent* camComp = m_CurrentSceneRendered->GetRegistry().try_get<Vulture::CameraComponent>(cameraEntity);
	CameraScript* camScript;

	if (scComp)
	{
		camScript = scComp->GetScript<CameraScript>(0);
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspaceID = ImGui::GetID("Dockspace");
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::DockSpaceOverViewport(viewport);
	}

	ImGui::Begin("Settings");
	if (!m_DrawIntoAFile)
	{
		ImGui::SeparatorText("Info");
		ImGui::Text("ms %f | fps %f", m_Timer.ElapsedMillis(), 1.0f / m_Timer.ElapsedSeconds());
		ImGui::Text("Total vertices: %i", m_CurrentSceneRendered->GetVertexCount());
		ImGui::Text("Total indices: %i", m_CurrentSceneRendered->GetIndexCount());

		ImGui::Text("Time: %fs", m_Time);
		ImGui::Text("Samples Per Pixel: %i", m_CurrentSamplesPerPixel);
		ImGui::SeparatorText("Info");

		m_Timer.Reset();

		if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::Separator();
			if (camScript)
			{
				if (ImGui::Button("Reset Camera"))
				{
					camScript->Reset();
					FixCameraAspectRatio();
				}
				ImGui::SliderFloat("Movement Speed", &camScript->m_MovementSpeed, 0.0f, 20.0f);
				ImGui::SliderFloat("Rotation Speed", &camScript->m_RotationSpeed, 0.0f, 4.0f);
			}
			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("Environment Map"))
		{
			ImGui::Separator();

			if (ImGui::SliderFloat("Azimuth", &m_DrawInfo.EnvAzimuth, 0.0f, 360.0f)) { ResetFrame(); };
			if (ImGui::SliderFloat("Altitude", &m_DrawInfo.EnvAltitude, 0.0f, 360.0f)) { ResetFrame(); };

			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("Post Processing"))
		{
			ImGui::Separator();
			ImGui::SeparatorText("Bloom");
			ImGui::SliderFloat("Threshold",		&m_DrawInfo.BloomInfo.Threshold, 0.0f, 3.0f);
			ImGui::SliderFloat("Strength",		&m_DrawInfo.BloomInfo.Strength, 0.0f, 3.0f);
			ImGui::SliderInt("Mip Count",		&m_DrawInfo.BloomInfo.MipCount, 0, 10);

			ImGui::SeparatorText("Tonemapping");
			ImGui::SliderFloat("Exposure",		&m_DrawInfo.TonemapInfo.Exposure, 0.0f, 3.0f);
			ImGui::SliderFloat("Contrast",		&m_DrawInfo.TonemapInfo.Contrast, 0.0f, 3.0f);
			ImGui::SliderFloat("Brightness",	&m_DrawInfo.TonemapInfo.Brightness, 0.0f, 3.0f);
			ImGui::SliderFloat("Saturation",	&m_DrawInfo.TonemapInfo.Saturation, 0.0f, 3.0f);
			ImGui::SliderFloat("Vignette",		&m_DrawInfo.TonemapInfo.Vignette, 0.0f, 1.0f);
			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("Path tracing"))
		{
			ImGui::Separator();

			ImGui::SliderInt("Max Depth",			&m_DrawInfo.RayDepth, 0, 20);
			ImGui::SliderInt("Samples Per Pixel",	&m_DrawInfo.TotalSamplesPerPixel, 1, 50'000);
			ImGui::SliderInt("Samples Per Frame",	&m_DrawInfo.SamplesPerFrame, 0, 40);

			ImGui::SliderFloat("Focal Length",		&m_DrawInfo.FocalLength, 1.0f, 100.0f);
			ImGui::SliderFloat("DoF Strength",		&m_DrawInfo.DOFStrength, 0.0f, 100.0f);
			if (ImGui::Button("Reset"))
			{
				ResetFrame();
			}
			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("Denoiser"))
		{
			ImGui::Separator();

			if (m_Denoised)
			{
				if (ImGui::Checkbox("Show Denoised Image", &m_ShowDenoised))
				{
					if (m_ShowDenoised)
					{
						m_PresentedImage = m_DenoisedImage;
						{
							CreateHDRSet();
						}
					}
					else
					{
						m_PresentedImage = m_TonemappedImage;
						{
							CreateHDRSet();
						}
					}
				}
			}

			if (ImGui::Button("Denoise"))
			{
				m_RunDenoising = true;
			}

			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("File"))
		{
			ImGui::Separator();
			if (ImGui::Button("Render to a file"))
				ImGui::OpenPopup("FileRenderPopup");

			if (ImGui::BeginPopup("FileRenderPopup"))
			{
				ImGui::SeparatorText("----------------------------------------------");


				ImGui::InputInt2("Resolution",					m_DrawFileInfo.Resolution);

				if (ImGui::Button("Copy Current Values"))
				{
					m_DrawFileInfo.DrawInfo = m_DrawInfo;
				}
				ImGui::SeparatorText("Path Tracing");
				ImGui::SliderInt("Max Ray Depth",				&m_DrawFileInfo.DrawInfo.RayDepth, 1, 20);
				ImGui::InputInt("Samples Per Frame",			&m_DrawFileInfo.DrawInfo.SamplesPerFrame);
				ImGui::InputInt("Total Samples Per Pixel",		&m_DrawFileInfo.DrawInfo.TotalSamplesPerPixel);
				ImGui::SliderFloat("Depth Of Field Strength",	&m_DrawFileInfo.DrawInfo.DOFStrength, 0.0f, 200.0f);
				ImGui::SliderFloat("Focal Length",				&m_DrawFileInfo.DrawInfo.FocalLength, 5.0f, 20.0f);

				ImGui::SeparatorText("Bloom");
				ImGui::SliderFloat("Threshold",		&m_DrawFileInfo.DrawInfo.BloomInfo.Threshold, 0.0f, 3.0f);
				ImGui::SliderFloat("Strength",		&m_DrawFileInfo.DrawInfo.BloomInfo.Strength, 0.0f, 3.0f);
				ImGui::SliderInt("Mip Count",		&m_DrawFileInfo.DrawInfo.BloomInfo.MipCount, 0, 10);

				ImGui::SeparatorText("Tonemapping");
				ImGui::SliderFloat("Exposure",		&m_DrawFileInfo.DrawInfo.TonemapInfo.Exposure, 0.0f, 3.0f);
				ImGui::SliderFloat("Contrast",		&m_DrawFileInfo.DrawInfo.TonemapInfo.Contrast, 0.0f, 3.0f);
				ImGui::SliderFloat("Brightness",	&m_DrawFileInfo.DrawInfo.TonemapInfo.Brightness, 0.0f, 3.0f);
				ImGui::SliderFloat("Saturation",	&m_DrawFileInfo.DrawInfo.TonemapInfo.Saturation, 0.0f, 3.0f);
				ImGui::SliderFloat("Vignette",		&m_DrawFileInfo.DrawInfo.TonemapInfo.Vignette, 0.0f, 1.0f);

				ImGui::Separator();
				if (ImGui::Button("Render"))
				{
					m_DrawIntoAFile = true;
					m_DrawIntoAFileChanged = true;
					camScript->m_CameraLocked = true;
					
					m_DrawInfo = m_DrawFileInfo.DrawInfo;

					m_ViewportSize = { (unsigned int)m_DrawFileInfo.Resolution[0], (unsigned int)m_DrawFileInfo.Resolution[1] };
				}
				ImGui::EndPopup();
			}
			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("GBuffer"))
		{
			ImGui::Separator();
			ImVec2 availSpace = ImGui::GetContentRegionAvail();
			ImGui::Text("Albedo");
			float aspectRatio = camComp->GetAspectRatio();
			ImGui::Image(m_ImGuiAlbedoDescriptor, { 300 * aspectRatio, 300 });
			ImGui::Text("Normal");
			ImGui::Image(m_ImGuiNormalDescriptor, { 300 * aspectRatio, 300 });
			ImGui::Text("Roughness & Metallness");
			ImGui::Image(m_ImGuiRoughnessDescriptor, { 300 * aspectRatio, 300 });
			ImGui::Text("Emissive");
			ImGui::Image(m_ImGuiEmissiveDescriptor, { 300 * aspectRatio, 300 });
			ImGui::Separator();
		}
	}
	else
	{
		ImGui::Text("ms %f | fps %f", m_Timer.ElapsedMillis(), 1.0f / m_Timer.ElapsedSeconds());
		ImGui::Text("Total vertices: %i", m_CurrentSceneRendered->GetVertexCount());
		ImGui::Text("Total indices: %i", m_CurrentSceneRendered->GetIndexCount());
		m_Timer.Reset();
		ImGui::Separator();
		ImGui::Text("Time: %fs", m_Time);
		ImGui::Text("Current Samples Per Pixel: %i", m_CurrentSamplesPerPixel);
		ImGui::Text("Total Samples Per Pixel: %i", m_DrawInfo.TotalSamplesPerPixel);

		if (m_DrawFileInfo.DrawingFramebufferFinished)
		{
			ImGui::SeparatorText("Finished!");
			ImGui::Text("Finished!");
			ImGui::Text("File Saved To PathTracer/Rendered_Images");

			if (ImGui::Button("Go Back"))
			{
				m_DrawFileInfo.DrawingFramebufferFinished = false;
				m_DrawIntoAFile = false;
				m_DrawIntoAFileChanged = true;
			}
		}
		else
		{
			if (ImGui::Button("Cancel"))
			{
				m_DrawFileInfo.DrawingFramebufferFinished = false;
				m_DrawIntoAFile = false;
				m_DrawIntoAFileChanged = true;
			}
		}
	}

	ImGui::End();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Preview Viewport");

	if (camScript && !m_DrawIntoAFile)
	{
		if (ImGui::IsWindowHovered() && (ImGui::IsWindowDocked()))
			camScript->m_CameraLocked = false;
		else
			camScript->m_CameraLocked = true;
	}

	ImVec2 viewportSize = ImGui::GetContentRegionAvail();
	ImGui::Image(m_ImGuiViewportDescriptor, viewportSize);
	static VkExtent2D prevViewportSize = m_ImGuiViewportSize;
	m_ImGuiViewportSize = { (unsigned int)viewportSize.x, (unsigned int)viewportSize.y };
	if (m_ImGuiViewportSize.width != prevViewportSize.width || m_ImGuiViewportSize.height != prevViewportSize.height)
	{
		m_ImGuiViewportResized = true;
		prevViewportSize = m_ImGuiViewportSize;
	}
	if (!m_DrawIntoAFile)
		m_ViewportSize = m_ImGuiViewportSize;

	ImGui::End();
	ImGui::PopStyleVar();

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Vulture::Renderer::GetCurrentCommandBuffer());
}
