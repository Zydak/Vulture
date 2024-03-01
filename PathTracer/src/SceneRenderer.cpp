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

	Vulture::Tonemap::CreateInfo tonemapInfo{};
	tonemapInfo.InputImages = { m_BloomImage };
	tonemapInfo.OutputImages = { m_TonemappedImage };
	m_Tonemapper.Init(tonemapInfo);

	Vulture::Bloom::CreateInfo bloomInfo{};
	bloomInfo.InputImages = { m_PathTracingImage };
	bloomInfo.OutputImages = { m_BloomImage };
	bloomInfo.MipsCount = m_DrawInfo.BloomInfo.MipCount;
	m_Bloom.Init(bloomInfo);

	bloomInfo.InputImages = { m_DenoisedImage };
	bloomInfo.OutputImages = { m_BloomImage };
	m_DenoisedBloom.Init(bloomInfo);

	m_PresentedImage = m_PathTracingImage;

	CreateDescriptorSets();
	CreatePipelines();

	m_Denoiser = std::make_shared<Vulture::Denoiser>();
	m_Denoiser->Init();
	m_Denoiser->AllocateBuffers(m_ViewportContentSize);

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

	auto view = m_CurrentSceneRendered->GetRegistry().view<Vulture::ModelComponent>();
	for (auto entity : view)
	{
		CurrentModelEntity = { entity, m_CurrentSceneRendered };
	}

	if (m_DrawIntoAFile)
		m_PresentedImage = m_PathTracingImage;
	else
		m_PresentedImage = m_TonemappedImage;

	if (m_RecreateRtPipeline)
	{
		vkDeviceWaitIdle(Vulture::Device::GetDevice());
		ResetFrame();
		CreateRayTracingPipeline();
		m_RecreateRtPipeline = false;
	}

	if (m_RecompileShader)
	{
		m_RecompileShader = false;
		CreateRayTracingPipeline();
		ResetFrame();
	}

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

	if (m_ChangeSkybox)
	{
		m_ChangeSkybox = false;
		vkDeviceWaitIdle(Vulture::Device::GetDevice());

		m_CurrentSceneRendered->DestroyEntity(m_CurrentSkyboxEntity);

		m_CurrentSkyboxEntity = m_CurrentSceneRendered->CreateEntity();
		auto& skyboxComponent = m_CurrentSkyboxEntity.AddComponent<Vulture::SkyboxComponent>(m_SkyboxPath);

		SetSkybox(m_CurrentSkyboxEntity);
	}

	if (m_ModelChanged)
	{
		m_ModelChanged = false;
		vkDeviceWaitIdle(Vulture::Device::GetDevice());

		auto view = m_CurrentSceneRendered->GetRegistry().view<Vulture::ModelComponent>();
		for (auto entity : view)
		{
			m_CurrentSceneRendered->GetRegistry().destroy(entity);
		}
		m_CurrentSceneRendered->ResetMeshCount();
		m_CurrentSceneRendered->ResetModelCount();
		m_CurrentSceneRendered->ResetVertexCount();
		m_CurrentSceneRendered->ResetIndexCount();

		CurrentModelEntity = m_CurrentSceneRendered->CreateModel(m_ModelPath, Vulture::Transform(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(m_ModelScale)));

		m_CurrentSceneRendered->InitAccelerationStructure();
		CreateRayTracingDescriptorSets(*m_CurrentSceneRendered);

		ResetFrame();
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
					if (m_DrawFileInfo.Denoise)
					{
						Denoise();
						m_PresentedImage = m_TonemappedImage;
					}
					m_DenoisedBloom.Run(m_DrawInfo.BloomInfo, Vulture::Renderer::GetCurrentCommandBuffer());
					m_Tonemapper.Run(m_DrawInfo.TonemapInfo, Vulture::Renderer::GetCurrentCommandBuffer());

					m_BloomImage->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, Vulture::Renderer::GetCurrentCommandBuffer());
					m_TonemappedImage->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Vulture::Renderer::GetCurrentCommandBuffer());

					CreateHDRSet();
					finished = true;
					m_DrawIntoAFileFinished = true;
					m_ToneMapped = true;
				}
			}
			else
			{
				m_PathTracingImage->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Vulture::Renderer::GetCurrentCommandBuffer());
			}

			Vulture::Renderer::ImGuiPass();

			if (!rayTracingFinished)
			{
				m_PathTracingImage->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, Vulture::Renderer::GetCurrentCommandBuffer());
			}

			Vulture::Renderer::EndFrame();

			if (finished)
			{
				vkDeviceWaitIdle(Vulture::Device::GetDevice());
				Vulture::Renderer::SaveImageToFile("", m_PresentedImage);

				m_PresentedImage->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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

		// Run denoised if requested
		if (m_RunDenoising)
		{
			Denoise();
			m_RunDenoising = false;
			m_Denoised = true;
		}

		if (m_ShowDenoised)
			m_DenoisedBloom.Run(m_DrawInfo.BloomInfo, Vulture::Renderer::GetCurrentCommandBuffer());
		else
			m_Bloom.Run(m_DrawInfo.BloomInfo, Vulture::Renderer::GetCurrentCommandBuffer());

		m_Tonemapper.Run(m_DrawInfo.TonemapInfo, Vulture::Renderer::GetCurrentCommandBuffer());

		m_BloomImage->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, Vulture::Renderer::GetCurrentCommandBuffer());
		m_TonemappedImage->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Vulture::Renderer::GetCurrentCommandBuffer());

		Vulture::Renderer::ImGuiPass();
		
		if (!Vulture::Renderer::EndFrame())
			RecreateResources();
	}
	else
	{
		RecreateResources();
	}
}

void SceneRenderer::RecreateRayTracingDescriptorSets()
{
	m_RayTracingDescriptorSet->UpdateImageSampler(1, Vulture::Renderer::GetSamplerHandle(), m_PathTracingImage->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
}

// TODO descritpion
bool SceneRenderer::RayTrace(const glm::vec4& clearColor)
{
	Vulture::Device::InsertLabel(Vulture::Renderer::GetCurrentCommandBuffer(), "Inserted label", { 0.0f, 1.0f, 0.0f, 1.0f });

	m_PushContantRayTrace.GetDataPtr()->ClearColor = clearColor;
	m_PushContantRayTrace.GetDataPtr()->maxDepth = m_DrawInfo.RayDepth;
	m_PushContantRayTrace.GetDataPtr()->FocalLength = m_DrawInfo.FocalLength;
	m_PushContantRayTrace.GetDataPtr()->DoFStrength = m_DrawInfo.DOFStrength;
	m_PushContantRayTrace.GetDataPtr()->AliasingJitter = m_DrawInfo.AliasingJitterStr;
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

	Vulture::Device::BeginLabel(Vulture::Renderer::GetCurrentCommandBuffer(), "Ray Trace Pass", { 1.0f, 0.0f, 0.0f, 1.0f });
	
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

	Vulture::Device::EndLabel(Vulture::Renderer::GetCurrentCommandBuffer());

	if (m_DrawInfo.AutoDoF)
	{
		memcpy(&m_DrawInfo.FocalLength, m_RayTracingDescriptorSet->GetBuffer(8)->GetMappedMemory(), sizeof(float));
	}

	return true;
}

void SceneRenderer::DrawGBuffer()
{
	if (!m_DrawGBuffer)
		return;

	Vulture::Device::BeginLabel(Vulture::Renderer::GetCurrentCommandBuffer(), "GBuffer rasterization", { 0.0f, 0.0f, 1.0f, 1.0f });

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

	m_GBufferFramebuffer->Bind(Vulture::Renderer::GetCurrentCommandBuffer(), clearColors);
	m_GBufferPipeline.Bind(Vulture::Renderer::GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS);

	m_GlobalDescriptorSets[Vulture::Renderer::GetCurrentFrameIndex()]->Bind
	(
		0,
		m_GBufferPipeline.GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);


	for (auto& entity : view)
	{
		auto& [modelComp, TransformComp] = m_CurrentSceneRendered->GetRegistry().get<Vulture::ModelComponent, Vulture::TransformComponent>(entity);
		
		std::vector<Vulture::Ref<Vulture::Mesh>> meshes = modelComp.Model->GetMeshes();
		std::vector<Vulture::Ref<Vulture::DescriptorSet>> sets = modelComp.Model->GetDescriptors();
		for (uint32_t i = 0; i < modelComp.Model->GetMeshCount(); i++)
		{
			m_PushContantGBuffer.GetDataPtr()->Material = modelComp.Model->GetMaterial(i);
			m_PushContantGBuffer.GetDataPtr()->Model = TransformComp.transform.GetMat4();
			
			m_PushContantGBuffer.Push(m_GBufferPipeline.GetPipelineLayout(), Vulture::Renderer::GetCurrentCommandBuffer());

			sets[i]->Bind(1, m_GBufferPipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_GRAPHICS, Vulture::Renderer::GetCurrentCommandBuffer());
			meshes[i]->Bind(Vulture::Renderer::GetCurrentCommandBuffer());
			meshes[i]->Draw(Vulture::Renderer::GetCurrentCommandBuffer(), 1, 0);
		}
	}

	m_GBufferFramebuffer->Unbind(Vulture::Renderer::GetCurrentCommandBuffer());

	Vulture::Device::EndLabel(Vulture::Renderer::GetCurrentCommandBuffer());
}

void SceneRenderer::Denoise()
{
	VkCommandBuffer cmd = Vulture::Renderer::GetCurrentCommandBuffer();

	// copy images to cuda buffers
	std::vector<Vulture::Image*> vec = 
	{
		&(*m_PathTracingImage), // Path Tracing Result
		const_cast<Vulture::Image*>((m_GBufferFramebuffer->GetImageNoVk(GBufferImage::Albedo))), // Albedo
		const_cast<Vulture::Image*>((m_GBufferFramebuffer->GetImageNoVk(GBufferImage::Normal))) // Normal
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

	Vulture::Tonemap::CreateInfo tonemapInfo{};
	tonemapInfo.InputImages = { m_BloomImage };
	tonemapInfo.OutputImages = { m_TonemappedImage };
	m_Tonemapper.Init(tonemapInfo);

	Vulture::Bloom::CreateInfo bloomInfo{};
	bloomInfo.InputImages = { m_PathTracingImage };
	bloomInfo.OutputImages = { m_BloomImage };
	bloomInfo.MipsCount = m_DrawInfo.BloomInfo.MipCount;
	m_Bloom.Init(bloomInfo);

	bloomInfo.InputImages = { m_DenoisedImage };
	bloomInfo.OutputImages = { m_BloomImage };
	m_DenoisedBloom.Init(bloomInfo);

	CreatePipelines();

	FixCameraAspectRatio();

	m_Denoiser->AllocateBuffers(m_ViewportContentSize);

	RecreateDescriptorSets();
}

void SceneRenderer::FixCameraAspectRatio()
{
	float newAspectRatio = (float)m_ViewportContentSize.width / (float)m_ViewportContentSize.height;
	auto view = m_CurrentSceneRendered->GetRegistry().view<Vulture::CameraComponent>();
	for (auto entity : view)
	{
		auto& cameraCp = view.get<Vulture::CameraComponent>(entity);
		cameraCp.SetPerspectiveMatrix(cameraCp.FOV, newAspectRatio, 0.1f, 100.0f);
	}
}

void SceneRenderer::CreateRenderPasses()
{
	// none for now
}

void SceneRenderer::CreateDescriptorSets()
{
	m_GlobalDescriptorSets.clear();

	CreateHDRSet();

	// Global Set
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin2{ 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

		m_GlobalDescriptorSets.push_back(std::make_shared<Vulture::DescriptorSet>());
		m_GlobalDescriptorSets[i]->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1, bin2 });
		m_GlobalDescriptorSets[i]->AddUniformBuffer(0, sizeof(GlobalUbo));

		m_GlobalDescriptorSets[i]->AddImageSampler(
			1,
			Vulture::Renderer::GetSamplerHandle(),
			Vulture::Renderer::GetEnv()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		m_GlobalDescriptorSets[i]->AddStorageBuffer(2, (uint32_t)Vulture::Renderer::GetEnv()->GetAccelBuffer()->GetBufferSize(), false, true);

		m_GlobalDescriptorSets[i]->Build();

		Vulture::Buffer::CopyBuffer(
			Vulture::Renderer::GetEnv()->GetAccelBuffer()->GetBuffer(),
			m_GlobalDescriptorSets[i]->GetBuffer(2)->GetBuffer(),
			Vulture::Renderer::GetEnv()->GetAccelBuffer()->GetBufferSize(),
			0, 0,
			Vulture::Device::GetGraphicsQueue(),
			0,
			Vulture::Device::GetGraphicsCommandPool()
		);
	}

	
#ifdef VL_IMGUI

	m_ImGuiAlbedoDescriptor = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_GBufferFramebuffer->GetImageView(GBufferImage::Albedo), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiNormalDescriptor = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_GBufferFramebuffer->GetImageView(GBufferImage::Normal), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiRoughnessDescriptor = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_GBufferFramebuffer->GetImageView(GBufferImage::RoughnessMetallness), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiEmissiveDescriptor = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_GBufferFramebuffer->GetImageView(GBufferImage::Emissive), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiViewportDescriptorTonemapped = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_TonemappedImage->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiViewportDescriptorPathTracing = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_PathTracingImage->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

#endif
}

void SceneRenderer::CreateRayTracingDescriptorSets(Vulture::Scene& scene)
{
	uint32_t texturesCount = scene.GetMeshCount();
	{
		Vulture::DescriptorSetLayout::Binding bin0{ 0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin2{ 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin3{ 3, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin4{ 4, texturesCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin5{ 5, texturesCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin6{ 6, texturesCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin7{ 7, texturesCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin8{ 8, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR };
		
		m_RayTracingDescriptorSet = std::make_shared<Vulture::DescriptorSet>();
		m_RayTracingDescriptorSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin0, bin1, bin2, bin3, bin4, bin5, bin6, bin7, bin8 });

		VkAccelerationStructureKHR tlas = scene.GetAccelerationStructure()->GetTlas().Accel;
		VkWriteDescriptorSetAccelerationStructureKHR asInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		asInfo.accelerationStructureCount = 1;
		asInfo.pAccelerationStructures = &tlas;

		m_RayTracingDescriptorSet->AddAccelerationStructure(0, asInfo);
		m_RayTracingDescriptorSet->AddImageSampler(1, Vulture::Renderer::GetSamplerHandle(), m_PathTracingImage->GetImageView(), VK_IMAGE_LAYOUT_GENERAL);
		m_RayTracingDescriptorSet->AddStorageBuffer(2, sizeof(MeshAdresses) * 50000, false, true);
		m_RayTracingDescriptorSet->AddStorageBuffer(3, sizeof(Vulture::Material) * 50000, false, true);

		auto view = scene.GetRegistry().view<Vulture::ModelComponent>();
		for (auto& entity : view)
		{
			auto& modelComp = scene.GetRegistry().get<Vulture::ModelComponent>(entity);
			for (int j = 0; j < (int)modelComp.Model->GetAlbedoTextureCount(); j++)
			{
				m_RayTracingDescriptorSet->AddImageSampler(
					4,
					Vulture::Renderer::GetSamplerHandle(),
					modelComp.Model->GetAlbedoTexture(j)->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				);
			}
			for (int j = 0; j < (int)modelComp.Model->GetNormalTextureCount(); j++)
			{
				m_RayTracingDescriptorSet->AddImageSampler(
					5,
					Vulture::Renderer::GetSamplerHandle(),
					modelComp.Model->GetNormalTexture(j)->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				);
			}
			for (int j = 0; j < (int)modelComp.Model->GetRoughnessTextureCount(); j++)
			{
				m_RayTracingDescriptorSet->AddImageSampler(
					6,
					Vulture::Renderer::GetSamplerHandle(),
					modelComp.Model->GetRoughnessTexture(j)->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				);
			}
			for (int j = 0; j < (int)modelComp.Model->GetMetallnessTextureCount(); j++)
			{
				m_RayTracingDescriptorSet->AddImageSampler(
					7,
					Vulture::Renderer::GetSamplerHandle(),
					modelComp.Model->GetMetallnessTexture(j)->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				);
			}
			m_RayTracingDescriptorSet->AddStorageBuffer(8, sizeof(float));
		}
		
		m_RayTracingDescriptorSet->Build();
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
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

		VL_CORE_ASSERT(meshSizes, "No meshes found?");

		m_RayTracingDescriptorSet->GetBuffer(2)->WriteToBuffer(meshAddresses.data(), meshSizes, 0);
		m_RayTracingDescriptorSet->GetBuffer(3)->WriteToBuffer(materials.data(), materialSizes, 0);

		m_RayTracingDescriptorSet->GetBuffer(2)->Flush(meshSizes, 0);
		m_RayTracingDescriptorSet->GetBuffer(3)->Flush(materialSizes, 0);
	}

	CreateRayTracingPipeline();
	CreateShaderBindingTable();
}

void SceneRenderer::SetSkybox(Vulture::Entity& skyboxEntity)
{
	m_CurrentSkyboxEntity = skyboxEntity;
	Vulture::SkyboxComponent skybox = skyboxEntity.GetComponent<Vulture::SkyboxComponent>();

	m_GlobalDescriptorSets.clear();
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };
		Vulture::DescriptorSetLayout::Binding bin2{ 2, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR };

		m_GlobalDescriptorSets.push_back(std::make_shared<Vulture::DescriptorSet>());
		m_GlobalDescriptorSets[i]->Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1, bin2 });
		m_GlobalDescriptorSets[i]->AddUniformBuffer(0, sizeof(GlobalUbo));

		m_GlobalDescriptorSets[i]->AddImageSampler(
			1,
			Vulture::Renderer::GetSamplerHandle(),
			skybox.SkyboxImage->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);

		m_GlobalDescriptorSets[i]->AddStorageBuffer(2, (uint32_t)skybox.SkyboxImage->GetAccelBuffer()->GetBufferSize(), false, true);

		m_GlobalDescriptorSets[i]->Build();

		Vulture::Buffer::CopyBuffer(
			skybox.SkyboxImage->GetAccelBuffer()->GetBuffer(),
			m_GlobalDescriptorSets[i]->GetBuffer(2)->GetBuffer(),
			skybox.SkyboxImage->GetAccelBuffer()->GetBufferSize(),
			0, 0,
			Vulture::Device::GetGraphicsQueue(),
			0,
			Vulture::Device::GetGraphicsCommandPool()
		);
	}

	m_RecompileShader = true;
	m_HasEnvMap = true;
}

void SceneRenderer::RecreateDescriptorSets()
{
	m_Denoised = false;
	m_ShowDenoised = false;
	{
		CreateHDRSet();
	}

	{
		m_RayTracingDescriptorSet->UpdateImageSampler(
			1,
			Vulture::Renderer::GetSamplerHandle(),
			m_PathTracingImage->GetImageView(),
			VK_IMAGE_LAYOUT_GENERAL
		);
	}

	RecreateRayTracingDescriptorSets();

#ifdef VL_IMGUI
	ImGui_ImplVulkan_RemoveTexture(m_ImGuiAlbedoDescriptor);
	ImGui_ImplVulkan_RemoveTexture(m_ImGuiNormalDescriptor);
	ImGui_ImplVulkan_RemoveTexture(m_ImGuiRoughnessDescriptor);
	ImGui_ImplVulkan_RemoveTexture(m_ImGuiEmissiveDescriptor);
	ImGui_ImplVulkan_RemoveTexture(m_ImGuiViewportDescriptorTonemapped);
	ImGui_ImplVulkan_RemoveTexture(m_ImGuiViewportDescriptorPathTracing);

	m_ImGuiAlbedoDescriptor = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_GBufferFramebuffer->GetImageView(GBufferImage::Albedo), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiNormalDescriptor = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_GBufferFramebuffer->GetImageView(GBufferImage::Normal), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiRoughnessDescriptor = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_GBufferFramebuffer->GetImageView(GBufferImage::RoughnessMetallness), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiEmissiveDescriptor = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_GBufferFramebuffer->GetImageView(GBufferImage::Emissive), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiViewportDescriptorTonemapped = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_TonemappedImage->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	m_ImGuiViewportDescriptorPathTracing = ImGui_ImplVulkan_AddTexture(Vulture::Renderer::GetSamplerHandle(), m_PathTracingImage->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

#endif
}

void SceneRenderer::CreatePipelines()
{
	
}

void SceneRenderer::CreateRayTracingPipeline()
{
	vkDeviceWaitIdle(Vulture::Device::GetDevice());
	//
	// Ray Tracing
	//
	{
		Vulture::PushConstant<PushConstantRay>::CreateInfo pushInfo{};
		pushInfo.Stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
		
		m_PushContantRayTrace.Init(pushInfo);

		Vulture::Pipeline::RayTracingCreateInfo info{};
		info.PushConstants = m_PushContantRayTrace.GetRangePtr();
		Vulture::Shader shader1({ "src/shaders/raytrace.rgen", VK_SHADER_STAGE_RAYGEN_BIT_KHR });

		std::vector<std::string> macros;
		if (m_DrawInfo.UseAlbedo)
			macros.push_back("USE_ALBEDO");
		if (m_DrawInfo.UseNormalMaps)
			macros.push_back("USE_NORMAL_MAPS");
		if (m_DrawInfo.SampleEnvMap)
			macros.push_back("SAMPLE_ENV_MAP");
		if (m_DrawInfo.UseGlossy)
			macros.push_back("USE_GLOSSY");
		if (m_DrawInfo.UseGlass)
			macros.push_back("USE_GLASS");
		if (m_DrawInfo.UseClearcoat)
			macros.push_back("USE_CLEARCOAT");

		Vulture::Shader shader2({ m_CurrentHitShaderPath, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, macros });
		
		Vulture::Shader shader3({ "src/shaders/raytrace.rmiss", VK_SHADER_STAGE_MISS_BIT_KHR });
		info.RayGenShaders.push_back(&shader1);
		info.HitShaders.push_back(&shader2);
		info.MissShaders.push_back(&shader3);

		// Descriptor set layouts for the pipeline
		std::vector<VkDescriptorSetLayout> layouts
		{
			m_RayTracingDescriptorSet->GetDescriptorSetLayout()->GetDescriptorSetLayoutHandle(),
			m_GlobalDescriptorSets[0]->GetDescriptorSetLayout()->GetDescriptorSetLayoutHandle()
		};
		info.DescriptorSetLayouts = layouts;
		info.debugName = "Ray Tracing Pipeline";

		m_RtPipeline.Init(info);
	}

	// GBuffer
	{
		Vulture::Shader::CreateInfo shaderInfo{};
		shaderInfo.Filepath = "src/shaders/GBuffer.vert";
		shaderInfo.Type = VK_SHADER_STAGE_VERTEX_BIT;
		Vulture::Shader testShader(shaderInfo);
		{
			Vulture::DescriptorSetLayout::Binding bin1{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
			Vulture::DescriptorSetLayout::Binding bin2{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
			Vulture::DescriptorSetLayout::Binding bin3{ 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
			Vulture::DescriptorSetLayout::Binding bin4{ 3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };

			Vulture::DescriptorSetLayout texturesLayout({ bin1, bin2, bin3, bin4 });

			Vulture::PushConstant<PushConstantGBuffer>::CreateInfo pushInfo{};
			pushInfo.Stage = VK_SHADER_STAGE_VERTEX_BIT;

			m_PushContantGBuffer.Init(pushInfo);

			// Configure pipeline creation parameters
			Vulture::Pipeline::GraphicsCreateInfo info{};
			info.AttributeDesc = Vulture::Mesh::Vertex::GetAttributeDescriptions();
			info.BindingDesc = Vulture::Mesh::Vertex::GetBindingDescriptions();
			Vulture::Shader shader1({ "src/shaders/GBuffer.vert", VK_SHADER_STAGE_VERTEX_BIT });

			std::vector<std::string> macros;
			if (m_DrawInfo.UseAlbedo)
				macros.push_back("USE_ALBEDO");
			if (m_DrawInfo.UseNormalMaps)
				macros.push_back("USE_NORMAL_MAPS");
			if (m_DrawInfo.SampleEnvMap)
				macros.push_back("SAMPLE_ENV_MAP");

			Vulture::Shader shader2({ "src/shaders/GBuffer.frag", VK_SHADER_STAGE_FRAGMENT_BIT, macros });
			info.Shaders.push_back(&shader1);
			info.Shaders.push_back(&shader2);
			info.BlendingEnable = false;
			info.DepthTestEnable = true;
			info.CullMode = VK_CULL_MODE_NONE;
			info.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			info.Width = m_ViewportContentSize.width;
			info.Height = m_ViewportContentSize.height;
			info.PushConstants = m_PushContantGBuffer.GetRangePtr();
			info.ColorAttachmentCount = 4;
			info.RenderPass = m_GBufferFramebuffer->GetRenderPass();
			info.debugName = "GBuffer Pipeline";

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				m_GlobalDescriptorSets[0]->GetDescriptorSetLayout()->GetDescriptorSetLayoutHandle(),
				texturesLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;

			m_GBufferPipeline.Init(info);
		}
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
		info.Height = m_ViewportContentSize.height;
		info.Width = m_ViewportContentSize.width;
		info.LayerCount = 1;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.SamplerInfo = Vulture::SamplerInfo{};
		info.Type = Vulture::Image::ImageType::Image2D;
		info.DebugName = "Path Tracing Image";
		m_PathTracingImage = std::make_shared<Vulture::Image>(info);
		m_PathTracingImage->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL);
	}

	// Denoised
	{
		Vulture::Image::CreateInfo info{};
		info.Width = m_ViewportContentSize.width;
		info.Height = m_ViewportContentSize.height;
		info.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.LayerCount = 1;
		info.SamplerInfo = Vulture::SamplerInfo{};
		info.Type = Vulture::Image::ImageType::Image2D;
		info.DebugName = "Denoised Image";

		m_DenoisedImage = std::make_shared<Vulture::Image>(info);
		m_DenoisedImage->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL);
	}

	// GBuffer
	{
		std::vector<Vulture::FramebufferAttachment> attachments
		{ 
			Vulture::FramebufferAttachment::ColorRGBA32, // This has to be 32 per channel otherwise optix won't work
			Vulture::FramebufferAttachment::ColorRGBA32, // This has to be 32 per channel otherwise optix won't work
			Vulture::FramebufferAttachment::ColorRG8,
			Vulture::FramebufferAttachment::ColorRGBA32,
			Vulture::FramebufferAttachment::Depth
		};
		
		Vulture::Framebuffer::CreateInfo info{};
		info.AttachmentsFormats = &attachments;
		info.DepthFormat = Vulture::Swapchain::FindDepthFormat();
		info.Extent = m_ViewportContentSize;
		info.CustomBits = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		Vulture::Framebuffer::RenderPassCreateInfo rPassInfo{};
		info.RenderPassInfo = &rPassInfo;

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
		info.RenderPassInfo->Dependencies = dependencies;

		m_GBufferFramebuffer = std::make_shared<Vulture::Framebuffer>(info);
	}

	// Tonemapped
	{
		Vulture::Image::CreateInfo info{};
		info.Width = m_ViewportContentSize.width;
		info.Height = m_ViewportContentSize.height;
		info.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.LayerCount = 1;
		info.SamplerInfo = Vulture::SamplerInfo{};
		info.Type = Vulture::Image::ImageType::Image2D;
		info.DebugName = "Tonemapped Image";

		m_TonemappedImage = std::make_shared<Vulture::Image>();
		m_TonemappedImage->Init(info);
	}

	// Bloom
	{
		Vulture::Image::CreateInfo info{};
		info.Width = m_ViewportContentSize.width;
		info.Height = m_ViewportContentSize.height;
		info.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // TODO: check all usages in general, especially here, the transfer one
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.LayerCount = 1;
		info.SamplerInfo = Vulture::SamplerInfo{};
		info.Type = Vulture::Image::ImageType::Image2D;
		info.DebugName = "Bloom Image";

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

	m_ToneMappedImageSet = std::make_shared<Vulture::DescriptorSet>();
	m_ToneMappedImageSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin });
	m_ToneMappedImageSet->AddImageSampler(
		0,
		Vulture::Renderer::GetSamplerHandle(),
		m_TonemappedImage->GetImageView(),
		VK_IMAGE_LAYOUT_GENERAL
	);
	m_ToneMappedImageSet->Build();

	m_DenoisedImageSet = std::make_shared<Vulture::DescriptorSet>();
	m_DenoisedImageSet->Init(&Vulture::Renderer::GetDescriptorPool(), { bin });
	m_DenoisedImageSet->AddImageSampler(
		0,
		Vulture::Renderer::GetSamplerHandle(),
		m_DenoisedImage->GetImageView(),
		VK_IMAGE_LAYOUT_GENERAL
	);
	m_DenoisedImageSet->Build();
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

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::SetNextWindowSize({ (float)m_ViewportSize.width, (float)m_ViewportSize.height });
	ImGui::Begin("Preview Viewport");

	if (camScript && !m_DrawIntoAFile)
	{
		if (ImGui::IsWindowHovered() && (ImGui::IsWindowDocked()))
			camScript->m_CameraLocked = false;
		else
			camScript->m_CameraLocked = true;
	}

	ImVec2 viewportContentSize = ImGui::GetContentRegionAvail();
	if (m_DrawIntoAFile && m_DrawIntoAFileFinished && m_DrawFileInfo.Denoise)
		ImGui::Image(m_ImGuiViewportDescriptorTonemapped, viewportContentSize);
	if (m_DrawIntoAFile && m_DrawIntoAFileFinished && !m_DrawFileInfo.Denoise)
		ImGui::Image(m_ImGuiViewportDescriptorTonemapped, viewportContentSize);
	if (m_DrawIntoAFile)
		ImGui::Image(m_ImGuiViewportDescriptorPathTracing, viewportContentSize);
	else if (m_ShowTonemapped)
		ImGui::Image(m_ImGuiViewportDescriptorTonemapped, viewportContentSize);
	else
		ImGui::Image(m_ImGuiViewportDescriptorTonemapped, viewportContentSize);

	static VkExtent2D prevViewportSize = { 1920, 1080 };
	VkExtent2D imGuiViewportContentSize = { (unsigned int)viewportContentSize.x, (unsigned int)viewportContentSize.y };
	if (imGuiViewportContentSize.width != prevViewportSize.width || imGuiViewportContentSize.height != prevViewportSize.height)
	{
		m_ImGuiViewportResized = true;
		prevViewportSize = imGuiViewportContentSize;
	}
	if (!m_DrawIntoAFile)
		m_ViewportContentSize = imGuiViewportContentSize;

	m_ViewportSize = { (uint32_t)ImGui::GetWindowSize().x, (uint32_t)ImGui::GetWindowSize().y };

	ImGui::End();
	ImGui::PopStyleVar();

	ImGui::Begin("Settings");
	if (!m_DrawIntoAFile)
	{
		ImGui::SeparatorText("Info");
		ImGui::Text("ms %f | fps %f", m_Timer.ElapsedMillis(), 1.0f / m_Timer.ElapsedSeconds());
		ImGui::Text("Total vertices: %i", m_CurrentSceneRendered->GetVertexCount());
		ImGui::Text("Total indices: %i", m_CurrentSceneRendered->GetIndexCount());

		ImGui::Text("Time: %fs", m_Time);
		ImGui::Text("Samples Per Pixel: %i", m_CurrentSamplesPerPixel);

		if (ImGui::Button("Reset"))
		{
			ResetFrame();
		}

		ImGui::SeparatorText("Info");

		m_Timer.Reset();

		if (ImGui::CollapsingHeader("Viewport"))
		{
			ImGui::Separator();
			ImGui::Text("ImGui doesn't allow to change size in dockspace\nso you have to undock the window first");
			ImGui::InputInt2("Viewport Size", (int*)&m_ViewportSize);
			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("Scene"))
		{
			ImGui::InputFloat("Models Scale", &m_ModelScale);
			ImGui::SeparatorText("Current Model");

			std::vector<std::string> scenesString;
			std::vector<const char*> scenes;

			int i = 0;
			for (const auto& entry : std::filesystem::directory_iterator("assets/"))
			{
				if (entry.is_regular_file())
				{
					if (entry.path().extension() == ".gltf" || entry.path().extension() == ".obj")
					{
						i++;
					}
				}
			}

			scenesString.reserve(i);
			scenes.reserve(i);
			i = 0;
			for (const auto& entry : std::filesystem::directory_iterator("assets/"))
			{
				if (entry.is_regular_file())
				{
					if (entry.path().extension() == ".gltf" || entry.path().extension() == ".obj")
					{
						scenesString.push_back(entry.path().filename().string());
						scenes.push_back(scenesString[i].c_str());
						i++;
					}
				}
			}

			static int currentSceneItem = 0;
			if (ImGui::ListBox("Current Scene", &currentSceneItem, scenes.data(), (int)scenes.size(), scenes.size() > 10 ? 10 : (int)scenes.size()))
			{
				m_ModelPath = "assets/" + scenesString[currentSceneItem];
				m_ModelChanged = true;
			}

			ImGui::SeparatorText("Materials");

			Vulture::ModelComponent comp = CurrentModelEntity.GetComponent<Vulture::ModelComponent>();
			m_CurrentMaterials = &comp.Model->GetMaterials();
			m_CurrentMeshesNames = comp.Model->GetNames();

			std::vector<const char*> meshesNames(m_CurrentMeshesNames.size());
			for (int i = 0; i < meshesNames.size(); i++)
			{
				meshesNames[i] = m_CurrentMeshesNames[i].c_str();
			}

			static int currentMaterialItem = 0;
			ImGui::ListBox("Materials", &currentMaterialItem, meshesNames.data(), (int)meshesNames.size(), meshesNames.size() > 10 ? 10 : (int)meshesNames.size());

			ImGui::SeparatorText("Material Values");

			bool valuesChanged = false;
			if (ImGui::ColorEdit4("Albedo", (float*)&(*m_CurrentMaterials)[currentMaterialItem].Color)) { valuesChanged = true; };
			if (ImGui::ColorEdit3("Emissive", (float*)&(*m_CurrentMaterials)[currentMaterialItem].Emissive)) { valuesChanged = true; };
			if (ImGui::SliderFloat("Emissive Strength", &(*m_CurrentMaterials)[currentMaterialItem].Emissive.a, 0.0f, 10.0f)) { valuesChanged = true; };
			if (ImGui::SliderFloat("Roughness", &(*m_CurrentMaterials)[currentMaterialItem].Roughness, 0.0f, 1.0f)) { valuesChanged = true; };
			if (ImGui::SliderFloat("Metallic", &(*m_CurrentMaterials)[currentMaterialItem].Metallic, 0.0f, 1.0f)) { valuesChanged = true; };
			if (ImGui::SliderFloat("IOR", &(*m_CurrentMaterials)[currentMaterialItem].Ior, 1.001f, 2.0f)) { valuesChanged = true; };
			if (ImGui::SliderFloat("Clearcoat", &(*m_CurrentMaterials)[currentMaterialItem].Clearcoat, 0.0f, 1.0f)) { valuesChanged = true; };
			if (ImGui::SliderFloat("Clearcoat Roughness", &(*m_CurrentMaterials)[currentMaterialItem].ClearcoatRoughness, 0.0f, 1.0f)) { valuesChanged = true; };
			if (ImGui::SliderFloat("SubSurface", &(*m_CurrentMaterials)[currentMaterialItem].Subsurface, 0.0f, 1.0f)) { valuesChanged = true; };
			
			static bool editAll = false;
			ImGui::Checkbox("Edit All Values", &editAll);

			if (editAll)
			{
				for (int i = 0; i < (*m_CurrentMaterials).size(); i++)
				{
					(*m_CurrentMaterials)[i].Color = (*m_CurrentMaterials)[currentMaterialItem].Color;
					(*m_CurrentMaterials)[i].Roughness = (*m_CurrentMaterials)[currentMaterialItem].Roughness;
					(*m_CurrentMaterials)[i].Metallic = (*m_CurrentMaterials)[currentMaterialItem].Metallic;
					(*m_CurrentMaterials)[i].Clearcoat = (*m_CurrentMaterials)[currentMaterialItem].Clearcoat;
					(*m_CurrentMaterials)[i].ClearcoatRoughness = (*m_CurrentMaterials)[currentMaterialItem].ClearcoatRoughness;
					(*m_CurrentMaterials)[i].Ior = (*m_CurrentMaterials)[currentMaterialItem].Ior;
				}

				if (valuesChanged)
				{
					// Upload to GPU
					m_RayTracingDescriptorSet->GetBuffer(3)->WriteToBuffer(
						m_CurrentMaterials->data(),
						sizeof(Vulture::Material) * (*m_CurrentMaterials).size(),
						0
					);

					ResetFrame();
				}
			}
			else
			{
				if (valuesChanged)
				{
					// Upload to GPU
					m_RayTracingDescriptorSet->GetBuffer(3)->WriteToBuffer(
						((uint8_t*)m_CurrentMaterials->data()) + (uint8_t)sizeof(Vulture::Material) * (uint8_t)currentMaterialItem,
						sizeof(Vulture::Material),
						sizeof(Vulture::Material) * currentMaterialItem
					);

					ResetFrame();
				}
			}
			ImGui::Separator();
		}

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
				ImGui::SliderFloat("Rotation Speed", &camScript->m_RotationSpeed, 0.0f, 40.0f);

				Vulture::CameraComponent* cp = &camScript->GetComponent<Vulture::CameraComponent>();
				if (ImGui::SliderFloat("FOV", &cp->FOV, 10.0f, 45.0f))
				{
					ResetFrame();
				}

				float newAspectRatio = (float)m_ViewportContentSize.width / (float)m_ViewportContentSize.height;
				cp->SetPerspectiveMatrix(cp->FOV, newAspectRatio, 0.1f, 1000.0f);

				glm::vec3 position = cp->Translation;
				glm::vec3 rotation = cp->Rotation.GetAngles();
				bool changed = false;
				if (ImGui::InputFloat3("Position", (float*)&position)) { changed = true; };
				if (ImGui::InputFloat3("Rotation", (float*)&rotation)) { changed = true; };

				if (changed)
				{
					cp->Translation = position;
					cp->Rotation.SetAngles(rotation);

					cp->UpdateViewMatrix();
				}
			}
			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("Environment Map"))
		{
			ImGui::Separator();

			if (ImGui::SliderFloat("Azimuth", &m_DrawInfo.EnvAzimuth, 0.0f, 360.0f)) { ResetFrame(); };
			if (ImGui::SliderFloat("Altitude", &m_DrawInfo.EnvAltitude, 0.0f, 360.0f)) { ResetFrame(); };

			std::vector<std::string> envMapsString;
			std::vector<const char*> envMaps;

			int i = 0;
			for (const auto& entry : std::filesystem::directory_iterator("assets/"))
			{
				if (entry.is_regular_file()) 
				{
					if (entry.path().extension() == ".hdr") 
					{
						i++;
					}
				}
			}

			envMapsString.reserve(i);
			envMaps.reserve(i);
			i = 0;
			for (const auto& entry : std::filesystem::directory_iterator("assets/"))
			{
				if (entry.is_regular_file())
				{
					if (entry.path().extension() == ".hdr")
					{
						envMapsString.push_back(entry.path().filename().string());
						envMaps.push_back(envMapsString[i].c_str());
						i++;
					}
				}
			}

			static int currentItem = 0;
			if (ImGui::ListBox("Current Environment Map", &currentItem, envMaps.data(), (int)envMaps.size(), envMaps.size() > 10 ? 10 : (int)envMaps.size()))
			{
				m_SkyboxPath = "assets/" + envMapsString[currentItem];
				m_ChangeSkybox = true;
			}

			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("Post Processing"))
		{
			ImGui::Separator();
			ImGui::SeparatorText("Bloom");
			ImGui::SliderFloat("Threshold",		&m_DrawInfo.BloomInfo.Threshold, 0.0f, 3.0f);
			ImGui::SliderFloat("Strength",		&m_DrawInfo.BloomInfo.Strength, 0.0f, 3.0f);
			if (ImGui::SliderInt("Mip Count",	&m_DrawInfo.BloomInfo.MipCount, 1, 10))
			{
				m_Bloom.RecreateDescriptors(m_DrawInfo.BloomInfo.MipCount, ((Vulture::Renderer::GetCurrentFrameIndex() + 1) % MAX_FRAMES_IN_FLIGHT));
			}

			ImGui::SeparatorText("Tonemapping");
			ImGui::SliderFloat("Exposure",		&m_DrawInfo.TonemapInfo.Exposure, 0.0f, 3.0f);
			ImGui::SliderFloat("Contrast",		&m_DrawInfo.TonemapInfo.Contrast, 0.0f, 3.0f);
			ImGui::SliderFloat("Brightness",	&m_DrawInfo.TonemapInfo.Brightness, 0.0f, 3.0f);
			ImGui::SliderFloat("Saturation",	&m_DrawInfo.TonemapInfo.Saturation, 0.0f, 3.0f);
			ImGui::SliderFloat("Vignette",		&m_DrawInfo.TonemapInfo.Vignette, 0.0f, 1.0f);
			if (ImGui::Checkbox("LinearSpace", (bool*)&m_DrawInfo.TonemapInfo.LinearSpace))
			{
				VL_CORE_WARN("{}", m_DrawInfo.TonemapInfo.LinearSpace);
			}
			ImGui::Separator();
		}

		if (ImGui::CollapsingHeader("Path tracing"))
		{
			ImGui::Separator();

			static int currentItem = 0;
			int prevItem = currentItem;
			const char* items[] = { "Cook-Torrance", "Disney" };
			if (ImGui::ListBox("Current Model", &currentItem, items, IM_ARRAYSIZE(items), 2))
			{
				if (currentItem == 0)
					m_CurrentHitShaderPath = "src/shaders/CookTorrance.rchit";
				else if (currentItem == 1)
					m_CurrentHitShaderPath = "src/shaders/Disney.rchit";

				if (prevItem != currentItem)
				{
					m_RecreateRtPipeline = true;
				}
			}

			if(ImGui::Checkbox("Use Normal Maps (Experimental)", &m_DrawInfo.UseNormalMaps))
			{
				m_RecompileShader = true;
			}
			if (ImGui::Checkbox("Use Albedo", &m_DrawInfo.UseAlbedo))
			{
				m_RecompileShader = true;
			}
			if (ImGui::Checkbox("Use Glossy Reflections", &m_DrawInfo.UseGlossy))
			{
				m_RecompileShader = true;
			}
			if (ImGui::Checkbox("Use Glass", &m_DrawInfo.UseGlass))
			{
				m_RecompileShader = true;
			}
			if (ImGui::Checkbox("Use Clearcoat", &m_DrawInfo.UseClearcoat))
			{
				m_RecompileShader = true;
			}

			if (m_HasEnvMap)
			{
				if (ImGui::Checkbox("Sample Environment Map", &m_DrawInfo.SampleEnvMap))
				{
					m_RecompileShader = true;
				}
			}

			ImGui::Text("");

			ImGui::SliderInt("Max Depth",			&m_DrawInfo.RayDepth, 1, 20);
			ImGui::SliderInt("Samples Per Pixel",	&m_DrawInfo.TotalSamplesPerPixel, 1, 50'000);
			ImGui::SliderInt("Samples Per Frame",	&m_DrawInfo.SamplesPerFrame, 1, 40);

			if(ImGui::SliderFloat("Aliasing Jitter Strength", &m_DrawInfo.AliasingJitterStr, 0.0f, 1.0f)) { ResetFrame(); };
			if(ImGui::Checkbox("Auto Focal Length",		&m_DrawInfo.AutoDoF)) { ResetFrame(); };
			if(ImGui::SliderFloat("Focal Length",		&m_DrawInfo.FocalLength, 1.0f, 100.0f)) { ResetFrame(); };
			if(ImGui::SliderFloat("DoF Strength",		&m_DrawInfo.DOFStrength, 0.0f, 100.0f)) { ResetFrame(); };
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
						m_ShowTonemapped = false;
					}
					else
					{
						m_ShowTonemapped = true;
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
				ImGui::SeparatorText("Window");
				ImGui::InputInt2("Resolution",					m_DrawFileInfo.Resolution);
				ImGui::Checkbox("Denoise", &m_DrawFileInfo.Denoise);

				ImGui::SeparatorText("Camera");
				{
					Vulture::CameraComponent* cp = &camScript->GetComponent<Vulture::CameraComponent>();
					if (ImGui::SliderFloat("FOV", &cp->FOV, 10.0f, 45.0f))
					{
						ResetFrame();
					}

					float newAspectRatio = (float)m_ViewportContentSize.width / (float)m_ViewportContentSize.height;
					cp->SetPerspectiveMatrix(cp->FOV, newAspectRatio, 0.1f, 1000.0f);

					glm::vec3 position = cp->Translation;
					glm::vec3 rotation = cp->Rotation.GetAngles();
					bool changed = false;
					if (ImGui::InputFloat3("Position", (float*)&position)) { changed = true; };
					if (ImGui::InputFloat3("Rotation", (float*)&rotation)) { changed = true; };

					if (changed)
					{
						cp->Translation = position;
						cp->Rotation.SetAngles(rotation);

						cp->UpdateViewMatrix();
					}
				}

				ImGui::SeparatorText("Path Tracing");
				if (ImGui::Button("Copy Current Values"))
				{
					m_DrawFileInfo.DrawInfo = m_DrawInfo;
				}
				ImGui::SeparatorText("Path Tracing");
				ImGui::SliderInt("Max Ray Depth",				&m_DrawFileInfo.DrawInfo.RayDepth, 1, 20);
				ImGui::InputInt("Samples Per Frame",			&m_DrawFileInfo.DrawInfo.SamplesPerFrame);
				ImGui::InputInt("Total Samples Per Pixel",		&m_DrawFileInfo.DrawInfo.TotalSamplesPerPixel);
				ImGui::SliderFloat("Aliasing Jitter Strength",  &m_DrawFileInfo.DrawInfo.AliasingJitterStr, 0.0f, 1.0f);
				ImGui::SliderFloat("Focal Length",				&m_DrawFileInfo.DrawInfo.FocalLength, 1.0f, 100.0f);
				ImGui::SliderFloat("DoF Strength",				&m_DrawFileInfo.DrawInfo.DOFStrength, 0.0f, 100.0f);

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

					m_ViewportContentSize = { (unsigned int)m_DrawFileInfo.Resolution[0], (unsigned int)m_DrawFileInfo.Resolution[1] };
				
					m_ViewportContentSize = { (uint32_t)m_DrawFileInfo.Resolution[0], (uint32_t)m_DrawFileInfo.Resolution[1] };
					prevViewportSize = { (uint32_t)m_DrawFileInfo.Resolution[0], (uint32_t)m_DrawFileInfo.Resolution[1] };
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
				m_DrawIntoAFileFinished = false;
				m_DrawFileInfo.DrawingFramebufferFinished = false;
				m_DrawIntoAFile = false;
				m_DrawIntoAFileChanged = true;
				m_ViewportContentSize = { (uint32_t)m_DrawFileInfo.Resolution[0], (uint32_t)m_DrawFileInfo.Resolution[1] };
				prevViewportSize = { (uint32_t)m_DrawFileInfo.Resolution[0], (uint32_t)m_DrawFileInfo.Resolution[1] };
			}
		}
		else
		{
			if (ImGui::Button("Cancel"))
			{
				m_DrawIntoAFileFinished = false;
				m_DrawFileInfo.DrawingFramebufferFinished = false;
				m_DrawIntoAFile = false;
				m_DrawIntoAFileChanged = true;
				m_ViewportContentSize = { (uint32_t)m_DrawFileInfo.Resolution[0], (uint32_t)m_DrawFileInfo.Resolution[1] };
				prevViewportSize = { (uint32_t)m_DrawFileInfo.Resolution[0], (uint32_t)m_DrawFileInfo.Resolution[1] };
			}
		}
	}

	ImGui::End();

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Vulture::Renderer::GetCurrentCommandBuffer());
}
