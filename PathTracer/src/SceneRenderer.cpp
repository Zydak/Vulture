#include "pch.h"
#include "SceneRenderer.h"

#include "imgui.h"
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include "CameraScript.h"

SceneRenderer::SceneRenderer()
{
	CreateRenderPasses();

	CreateFramebuffers();
	CreateUniforms();
	CreatePipelines();

	Vulture::Renderer::RenderImGui([this](){ImGuiPass(); });
}

SceneRenderer::~SceneRenderer()
{

}

void SceneRenderer::Render(Vulture::Scene& scene)
{
	m_CurrentSceneRendered = &scene;

	if (Vulture::Renderer::BeginFrame())
	{
		UpdateUniformData();
		RayTrace(glm::vec4(0.1f));

		Vulture::Renderer::FramebufferCopyPass(&(*m_HDRUniforms[Vulture::Renderer::GetCurrentFrameIndex()]));

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
	PushConstantRay pcRay{};
	pcRay.ClearColor = clearColor;

	if (m_CircleLight)
	{
		double time = glfwGetTime();
		pcRay.LightPosition.x = 10.0f * (float)cos(time);
		pcRay.LightPosition.z = 10.0f * (float)sin(time);
	}
	else
	{
		pcRay.LightPosition = glm::vec4(0.0f, 0.0f, 10.0f, 1.0f);
	}

	m_RtPipeline.Bind(Vulture::Renderer::GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	m_RayTracingUniforms[Vulture::Renderer::GetCurrentFrameIndex()]->Bind(
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
		0, sizeof(PushConstantRay), &pcRay);

	Vulture::Device::vkCmdTraceRaysKHR(
		Vulture::Renderer::GetCurrentCommandBuffer(), 
		&m_RgenRegion, 
		&m_MissRegion, 
		&m_HitRegion, 
		&m_CallRegion,
		(uint32_t)m_HDRFramebuffer[0]->GetExtent().x,
		(uint32_t)m_HDRFramebuffer[0]->GetExtent().y,
		1
	);
}

void SceneRenderer::RecreateResources()
{
	CreateFramebuffers();
	RecreateUniforms();
	CreatePipelines();

	FixCameraAspectRatio();
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
		colorAttachment.format = VK_FORMAT_R16G16B16A16_UNORM;
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
		albedoAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
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

		VkAttachmentDescription roughnessAttachment = {};
		roughnessAttachment.format = VK_FORMAT_R8_UNORM;
		roughnessAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		roughnessAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		roughnessAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		roughnessAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		roughnessAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		roughnessAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		roughnessAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference roughnessAttachmentRef = {};
		roughnessAttachmentRef.attachment = 1;
		roughnessAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription metallnessAttachment = {};
		metallnessAttachment.format = VK_FORMAT_R8_UNORM;
		metallnessAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		metallnessAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		metallnessAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		metallnessAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		metallnessAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		metallnessAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		metallnessAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference metallnessAttachmentRef = {};
		metallnessAttachmentRef.attachment = 2;
		metallnessAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription normalAttachment = {};
		normalAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
		normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		normalAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

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
	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_HDRUniforms.push_back(std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool()));
		m_HDRUniforms[i]->AddImageSampler(0, Vulture::Renderer::GetSampler().GetSampler(), m_HDRFramebuffer[i]->GetColorImageView(0),
			VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT
		);
		m_HDRUniforms[i]->Build();
	}
}

void SceneRenderer::CreateRayTracingUniforms(Vulture::Scene& scene)
{
	m_RayTracingUniforms.clear();
	m_GlobalUniforms.clear();

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_RayTracingUniforms.push_back(std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool()));

		VkAccelerationStructureKHR tlas = scene.GetAccelerationStructure()->GetTlas().Accel;
		VkWriteDescriptorSetAccelerationStructureKHR asInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
		asInfo.accelerationStructureCount = 1;
		asInfo.pAccelerationStructures = &tlas;

		m_RayTracingUniforms[i]->AddAccelerationStructure(0, asInfo);
		m_RayTracingUniforms[i]->AddImageSampler(1, Vulture::Renderer::GetSampler().GetSampler(), m_HDRFramebuffer[i]->GetColorImageView(0),
			VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
		m_RayTracingUniforms[i]->AddStorageBuffer(2, sizeof(MeshAdresses) * 100, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, true);

		m_RayTracingUniforms[i]->Build();
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_GlobalUniforms.push_back(std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool()));
		m_GlobalUniforms[i]->AddUniformBuffer(0, sizeof(GlobalUbo), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);

		m_GlobalUniforms[i]->Build();
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		std::vector<MeshAdresses> meshAddresses;
		auto modelView = scene.GetRegistry().view<Vulture::ModelComponent, Vulture::TransformComponent>();
		uint32_t size = 0;
		for (auto& entity : modelView)
		{
			auto& [modelComp, transformComp] = scene.GetRegistry().get<Vulture::ModelComponent, Vulture::TransformComponent>(entity);
			for (int i = 0; i < (int)modelComp.Model.GetMeshCount(); i++)
			{
				MeshAdresses adr{};
				adr.VertexAddress = modelComp.Model.GetMesh(i).GetVertexBuffer()->GetDeviceAddress();
				adr.IndexAddress = modelComp.Model.GetMesh(i).GetIndexBuffer()->GetDeviceAddress();

				meshAddresses.push_back(adr);
			}
			size = sizeof(MeshAdresses) * modelComp.Model.GetMeshCount();
			m_RayTracingUniforms[i]->GetBuffer(2)->WriteToBuffer(meshAddresses.data(), size, 0);
		}
		if (!size)
			VL_CORE_ASSERT(false, "No meshes found?");

		m_RayTracingUniforms[i]->GetBuffer(2)->Flush(size, 0);
	}

	CreateRayTracingPipeline();
	CreateShaderBindingTable();
}

void SceneRenderer::RecreateUniforms()
{
	m_HDRUniforms.clear();
	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_HDRUniforms.push_back(std::make_shared<Vulture::Uniform>(Vulture::Renderer::GetDescriptorPool()));
		m_HDRUniforms[i]->AddImageSampler(0, Vulture::Renderer::GetSampler().GetSampler(), m_HDRFramebuffer[i]->GetColorImageView(0),
			VK_IMAGE_LAYOUT_GENERAL, VK_SHADER_STAGE_FRAGMENT_BIT
		);
		m_HDRUniforms[i]->Build();
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_RayTracingUniforms[i]->UpdateImageSampler(1, m_HDRFramebuffer[i]->GetColorImageView(0), Vulture::Renderer::GetSampler().GetSampler(),
			VK_IMAGE_LAYOUT_GENERAL);
	}
}

void SceneRenderer::CreatePipelines()
{

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
			m_RayTracingUniforms[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout(),
			m_GlobalUniforms[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout()
		};
		info.UniformSetLayouts = layouts;

		m_RtPipeline.CreateRayTracingPipeline(m_RtShaderGroups, info);
	}
}

void SceneRenderer::CreateShaderBindingTable()
{
	uint32_t missCount{ 1 };
	uint32_t hitCount{ 1 };
	auto     handleCount = 1 + missCount + hitCount;
	uint32_t handleSize = Vulture::Device::GetRayTracingProperties().shaderGroupHandleSize;

	// The SBT (buffer) need to have starting groups to be aligned and handles in the group to be aligned.
	uint32_t handleSizeAligned = (uint32_t)Vulture::Device::GetAlignment(handleSize, Vulture::Device::GetRayTracingProperties().shaderGroupHandleAlignment);

	m_RgenRegion.stride = Vulture::Device::GetAlignment(handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);
	m_RgenRegion.size = m_RgenRegion.stride;  // The size member of pRayGenShaderBindingTable must be equal to its stride member
	
	m_MissRegion.stride = handleSizeAligned;
	m_MissRegion.size = Vulture::Device::GetAlignment(missCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);
	
	m_HitRegion.stride = handleSizeAligned;
	m_HitRegion.size = Vulture::Device::GetAlignment(hitCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);

	// Get the shader group handles
	uint32_t             dataSize = handleCount * handleSize;
	std::vector<uint8_t> handles(dataSize);
	auto result = Vulture::Device::vkGetRayTracingShaderGroupHandlesKHR(Vulture::Device::GetDevice(), m_RtPipeline.GetPipeline(), 0, handleCount, dataSize, handles.data());
	assert(result == VK_SUCCESS);

	// Allocate a buffer for storing the SBT.
	VkDeviceSize sbtSize = m_RgenRegion.size + m_MissRegion.size + m_HitRegion.size + m_CallRegion.size;
	m_RtSBTBuffer = std::make_shared<Vulture::Buffer>(sbtSize, 1,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
	);

	// Find the SBT addresses of each group
	m_RgenRegion.deviceAddress = m_RtSBTBuffer->GetDeviceAddress();
	m_MissRegion.deviceAddress = m_RtSBTBuffer->GetDeviceAddress() + m_RgenRegion.size;
	m_HitRegion.deviceAddress = m_RtSBTBuffer->GetDeviceAddress() + m_RgenRegion.size + m_MissRegion.size;

	auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

	// TODO maybe use WriteToBuffer?
	m_RtSBTBuffer->Map();
	uint8_t* pSBTBuffer = reinterpret_cast<uint8_t*>(m_RtSBTBuffer->GetMappedMemory());
	uint8_t* pData{ nullptr };
	uint32_t handleIdx{ 0 };

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

	m_RtSBTBuffer->Unmap();
}

void SceneRenderer::CreateFramebuffers()
{
	m_HDRFramebuffer.clear();
	m_GBufferFramebuffer.clear();

	// HDR
	{
		std::vector<Vulture::FramebufferAttachment> attachments{ Vulture::FramebufferAttachment::ColorRGBA16 };
		for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_HDRFramebuffer.push_back(std::make_unique<Vulture::Framebuffer>(attachments, m_HDRPass.GetRenderPass(), Vulture::Renderer::GetSwapchain().GetSwapchainExtent(), Vulture::Swapchain::FindDepthFormat(), 1, Vulture::ImageType::Image2D, VK_IMAGE_USAGE_STORAGE_BIT));
			Vulture::Image::TransitionImageLayout(m_HDRFramebuffer[i]->GetColorImage(0), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		}
	}

	// GBuffer
	{
		std::vector<Vulture::FramebufferAttachment> attachments{ Vulture::FramebufferAttachment::ColorRGBA8, Vulture::FramebufferAttachment::ColorR8, Vulture::FramebufferAttachment::ColorR8, Vulture::FramebufferAttachment::ColorRGBA8, Vulture::FramebufferAttachment::Depth };
		for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			m_GBufferFramebuffer.push_back(std::make_unique<Vulture::Framebuffer>(attachments, m_GBufferPass.GetRenderPass(), Vulture::Renderer::GetSwapchain().GetSwapchainExtent(), Vulture::Swapchain::FindDepthFormat(), 1, Vulture::ImageType::Image2D));
		}
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

	ImGui::Text("ms %f | fps %f", m_Timer.ElapsedMillis(), 1.0f / m_Timer.Elapsed());
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
					if (ImGui::Checkbox("Orbit Camera", &camScript->orbitCamera) && camScript->orbitCamera == false)
					{
						comp.Translation = glm::vec3(0.0f, 0.0f, -10.0f);
					}
				}
			}

			break;
		}
	}

	ImGui::Separator();
	ImGui::Checkbox("Orbiting light", &m_CircleLight);

	ImGui::End();
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Vulture::Renderer::GetCurrentCommandBuffer());
}
