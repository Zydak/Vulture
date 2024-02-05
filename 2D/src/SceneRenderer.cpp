#include "pch.h"
#include "SceneRenderer.h"

#include "imgui.h"
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

SceneRenderer::SceneRenderer()
{
	CreateRenderPasses();

	CreateFramebuffers();
	CreateDescriptorSets();
	CreatePipelines();

	m_StorageBufferTransforms.resize(Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT);
	m_TextStorageBufferTransforms.resize(Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT);
	Vulture::Renderer::RenderImGui([this](){ImGuiPass(); });
}

SceneRenderer::~SceneRenderer()
{

}

void SceneRenderer::Render(Vulture::Scene& scene)
{
	m_CurrentSceneRendered = &scene;
	UpdateStorageBuffer();
	UpdateTextStorageBuffers();

	if (Vulture::Renderer::BeginFrame())
	{
		UpdateTextBuffers();
		GeometryPass();

		Vulture::Renderer::FramebufferCopyPassImGui(m_HDRDescriptorSet[Vulture::Renderer::GetCurrentFrameIndex()]);

		if (!Vulture::Renderer::EndFrame())
			RecreateResources();
	}
	else
	{
		RecreateResources();
	}
}

void SceneRenderer::RecreateResources()
{
	CreateFramebuffers();
	RecreateDescriptorSets();
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
		cameraCp.SetOrthographicMatrix({ -20.0f, 20.0f, -20.0f, 20.0f }, 0.1f, 100.0f, newAspectRatio);
	}
}

void SceneRenderer::UpdateStorageBuffer()
{
	// Retrieve entities with TransformComponent and SpriteComponent from the current scene
	auto view = m_CurrentSceneRendered->GetRegistry().view<Vulture::TransformComponent, Vulture::SpriteComponent>();

	// Iterate over entities and update storage buffer entries
	int i = 0;
	for (auto entity : view)
	{
		const auto& [transformComponent, sprite] = view.get<Vulture::TransformComponent, Vulture::SpriteComponent>(entity);

		// Create a storage buffer entry for the entity
		StorageBufferEntry entry{};
		if (i >= m_StorageBufferTransforms[Vulture::Renderer::GetCurrentFrameIndex()].size())
		{
			entry.ModelMatrix = transformComponent.transform.GetMat4();
			entry.AtlasOffset = glm::vec4(sprite.AtlasOffsets, 1.0f, 1.0f);

			// Add the entry to the storage buffer
			m_StorageBufferTransforms[Vulture::Renderer::GetCurrentFrameIndex()].push_back({transformComponent.transform, entity});
			m_ObjectsUbos[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->WriteToBuffer(&entry, sizeof(StorageBufferEntry), i * sizeof(StorageBufferEntry));
		}
		else
		{
			bool changed;
			entry.ModelMatrix = transformComponent.transform.GetMat4(m_StorageBufferTransforms[Vulture::Renderer::GetCurrentFrameIndex()][i].Transform, changed);

			if (changed)
			{
				m_StorageBufferTransforms[Vulture::Renderer::GetCurrentFrameIndex()][i].Transform = transformComponent.transform;
				entry.AtlasOffset = glm::vec4(sprite.AtlasOffsets, 1.0f, 1.0f);

				m_ObjectsUbos[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->WriteToBuffer(&entry, sizeof(StorageBufferEntry), i * sizeof(StorageBufferEntry));
			}
		}
		
		i++;
	}

	// Write the storage buffer data to the UBO buffer and flush it
	//m_ObjectsUbos[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->WriteToBuffer(m_StorageBuffer.data(), m_StorageBuffer.size() * sizeof(StorageBufferEntry), 0);
	m_ObjectsUbos[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->Flush();

	// get camera matrix of the main camera
	auto cameraView = m_CurrentSceneRendered->GetRegistry().view<Vulture::CameraComponent>();
	MainUbo mainUbo{};
	for (auto cameraEntity : cameraView)
	{
		auto& cameraComponent = cameraView.get<Vulture::CameraComponent>(cameraEntity);
		if (cameraComponent.Main)
		{
			mainUbo.ViewProjMatrix = cameraComponent.GetViewProj();
			break;
		}
	}

	// Write the camera matrix data to the UBO buffer and flush it
	m_MainUbos[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->WriteToBuffer(&mainUbo, sizeof(MainUbo), 0);
	m_MainUbos[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->Flush();
}

void SceneRenderer::UpdateTextStorageBuffers()
{
	// Retrieve entities with TransformComponent and SpriteComponent from the current scene
	auto view = m_CurrentSceneRendered->GetRegistry().view<Vulture::TransformComponent, Vulture::TextComponent>();

	// Iterate over entities and update storage buffer entries
	int i = 0;
	for (auto entity : view)
	{
		const auto& [transformComponent, text] = view.get<Vulture::TransformComponent, Vulture::TextComponent>(entity);

		// Create a storage buffer entry for the entity
		glm::mat4 entry{};
		entry = transformComponent.transform.GetMat4();

		// Add the entry to the storage buffer
		m_TextUbos[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->WriteToBuffer(&entry, sizeof(glm::mat4), i * sizeof(glm::mat4));

		i++;
	}

	// Write the storage buffer data to the UBO buffer and flush it
	m_TextUbos[Vulture::Renderer::GetCurrentFrameIndex()]->GetBuffer(0)->Flush();
}

void SceneRenderer::UpdateTextBuffers()
{
	auto view = m_CurrentSceneRendered->GetRegistry().view<Vulture::TextComponent>();

	for (auto& entity : view) 
	{
		auto& textComponent = m_CurrentSceneRendered->GetRegistry().get<Vulture::TextComponent>(entity);

		if (textComponent.Text.IsResizable()) 
		{
			textComponent.Text.UploadToBuffer(Vulture::Renderer::GetCurrentFrameIndex(), Vulture::Renderer::GetCurrentCommandBuffer());

			// Memory barrier to synchronize transfer to vertex and index buffer
			VkBufferMemoryBarrier bufferMemoryBarrier = {};
			bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
			bufferMemoryBarrier.pNext = nullptr;
			bufferMemoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
			bufferMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			bufferMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			bufferMemoryBarrier.offset = 0;
			bufferMemoryBarrier.size = VK_WHOLE_SIZE;

			// Vertex Buffer
			bufferMemoryBarrier.buffer = textComponent.Text.GetTextMesh(Vulture::Renderer::GetCurrentFrameIndex()).GetVertexBuffer()->GetBuffer();  // Adjust accordingly
			vkCmdPipelineBarrier(
				Vulture::Renderer::GetCurrentCommandBuffer(),
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,
				0,
				nullptr,
				1,
				&bufferMemoryBarrier,
				0,
				nullptr
			);

			// Index Buffer
			bufferMemoryBarrier.buffer = textComponent.Text.GetTextMesh(Vulture::Renderer::GetCurrentFrameIndex()).GetIndexBuffer()->GetBuffer();  // Adjust accordingly
			vkCmdPipelineBarrier(
				Vulture::Renderer::GetCurrentCommandBuffer(),
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
				0,
				0,
				nullptr,
				1,
				&bufferMemoryBarrier,
				0,
				nullptr
			);
		}
	}
}

void SceneRenderer::UpdateStaticStorageBuffer(Vulture::Scene& scene)
{
	m_StaticObjectsCount = 0;
	m_StaticObjectsUbos = std::make_shared<Vulture::DescriptorSet>(Vulture::Renderer::GetDescriptorPool());
	m_StaticObjectsUbos->AddStorageBuffer(0, sizeof(StorageBufferEntry) * 1000, VK_SHADER_STAGE_VERTEX_BIT, true);
	m_StaticObjectsUbos->Build();

	// Retrieve entities with TransformComponent and SpriteComponent from the current scene
	auto view = scene.GetRegistry().view<Vulture::StaticTransformComponent, Vulture::SpriteComponent>();
	VL_CORE_ASSERT(view.size_hint() != 0, "Scenes does not contain any static objects. They probably got deleted by previous UpdateStaticStorageBuffer call");

	// Clear the storage buffer
	std::vector<StorageBufferEntry> staticStorageBuffer;

	// Iterate over entities and update storage buffer entries
	for (auto entity : view)
	{
		const auto& [transformComponent, sprite] = view.get<Vulture::StaticTransformComponent, Vulture::SpriteComponent>(entity);

		// Create a storage buffer entry for the entity
		StorageBufferEntry entry{};
		entry.ModelMatrix = transformComponent.transform.GetMat4();
		entry.AtlasOffset = glm::vec4(sprite.AtlasOffsets, 1.0f, 1.0f);

		// Add the entry to the storage buffer
		staticStorageBuffer.push_back(entry);
		scene.DestroyEntity(Vulture::Entity(entity, &scene));
		m_StaticObjectsCount++;
	}

	// Write the storage buffer data to the UBO buffer and flush it
	m_StaticObjectsUbos->GetBuffer(0)->WriteToBuffer(staticStorageBuffer.data(), staticStorageBuffer.size() * sizeof(StorageBufferEntry), 0);
	m_StaticObjectsUbos->GetBuffer(0)->Flush();
}

void SceneRenderer::DestroySprite(entt::entity entity, Vulture::Scene& scene)
{
	for (int i = 0; i < m_StorageBufferTransforms.size(); i++)
	{
		if (!m_StorageBufferTransforms[i].empty())
			m_StorageBufferTransforms[i].pop_back();
	}
	scene.DestroyEntity(Vulture::Entity(entity, &scene));
}

void SceneRenderer::GeometryPass()
{
	// Set up clear colors for the render pass
	std::vector<VkClearValue> clearColors;
	clearColors.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
	VkClearValue clearVal{};
	clearVal.depthStencil = { 1.0f, 1 };
	clearColors.push_back(clearVal);
	// Begin the render pass
	m_HDRPass.SetRenderTarget(&(*m_HDRFramebuffer[Vulture::Renderer::GetCurrentFrameIndex()]));
	m_HDRPass.BeginRenderPass(clearColors);

	// Bind UBOs and descriptor sets
	m_ObjectsUbos[Vulture::Renderer::GetCurrentFrameIndex()]->Bind(
		0,
		m_HDRPass.GetPipeline().GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);

	m_MainUbos[Vulture::Renderer::GetCurrentFrameIndex()]->Bind(
		1,
		m_HDRPass.GetPipeline().GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);

	m_CurrentSceneRendered->GetAtlas()->GetAtlasDescriptorSet()->Bind(
		2,
		m_HDRPass.GetPipeline().GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);

	// Bind and draw the quad mesh
	Vulture::Renderer::GetQuadMesh().Bind(Vulture::Renderer::GetCurrentCommandBuffer());
	Vulture::Renderer::GetQuadMesh().Draw(Vulture::Renderer::GetCurrentCommandBuffer(), (uint32_t)m_StorageBufferTransforms[Vulture::Renderer::GetCurrentFrameIndex()].size());

	if (m_StaticObjectsCount)
	{
		// Draw static objects
		m_StaticObjectsUbos->Bind(
			0,
			m_HDRPass.GetPipeline().GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		Vulture::Renderer::GetQuadMesh().Bind(Vulture::Renderer::GetCurrentCommandBuffer());
		Vulture::Renderer::GetQuadMesh().Draw(Vulture::Renderer::GetCurrentCommandBuffer(), (uint32_t)m_StaticObjectsCount);
	}

	// Text
	m_FontPipeline.Bind(Vulture::Renderer::GetCurrentCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS);
	m_MainUbos[Vulture::Renderer::GetCurrentFrameIndex()]->Bind(
		0,
		m_FontPipeline.GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);

	m_TextUbos[Vulture::Renderer::GetCurrentFrameIndex()]->Bind(
		1,
		m_FontPipeline.GetPipelineLayout(),
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		Vulture::Renderer::GetCurrentCommandBuffer()
	);

	glm::vec4 color(1.0f);
	vkCmdPushConstants(
		Vulture::Renderer::GetCurrentCommandBuffer(),
		m_FontPipeline.GetPipelineLayout(),
		VK_SHADER_STAGE_VERTEX_BIT,
		0,
		sizeof(glm::vec4),
		&color
	);

	auto textView = m_CurrentSceneRendered->GetRegistry().view<Vulture::TransformComponent, Vulture::TextComponent>();
	int i = 0;
	for (auto& entity : textView)
	{
		auto& textCp = m_CurrentSceneRendered->GetRegistry().get<Vulture::TextComponent>(entity);

		textCp.Text.GetFontAtlas()->GetUniform()->Bind(
			2,
			m_FontPipeline.GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		textCp.Text.GetTextMesh(Vulture::Renderer::GetCurrentFrameIndex()).Bind(Vulture::Renderer::GetCurrentCommandBuffer());
		textCp.Text.GetTextMesh(Vulture::Renderer::GetCurrentFrameIndex()).Draw(Vulture::Renderer::GetCurrentCommandBuffer(), 1, 0);
		i++;
	}

	// End the render pass
	m_HDRPass.EndRenderPass();
}

void SceneRenderer::CreateRenderPasses()
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

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = Vulture::Swapchain::FindDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
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

	std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };
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

void SceneRenderer::CreateDescriptorSets()
{
	// Create and initialize uniform buffers
	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Create and initialize uniform buffers
		m_ObjectsUbos.push_back(std::make_shared<Vulture::DescriptorSet>(Vulture::Renderer::GetDescriptorPool()));
		m_ObjectsUbos[i]->AddStorageBuffer(0, sizeof(StorageBufferEntry) * 200, VK_SHADER_STAGE_VERTEX_BIT, false);
		m_ObjectsUbos[i]->Build();

		// Resize test
		// TODO: automatic resize when needed
		//m_ObjectsUbos[i]->Resize(0, sizeof(StorageBufferEntry) * 200, Vulture::Device::GetGraphicsQueue(), Vulture::Device::GetGraphicsCommandPool());
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Create and initialize uniform buffers
		m_TextUbos.push_back(std::make_shared<Vulture::DescriptorSet>(Vulture::Renderer::GetDescriptorPool()));
		m_TextUbos[i]->AddStorageBuffer(0, sizeof(glm::mat4) * 100, VK_SHADER_STAGE_VERTEX_BIT, true);
		m_TextUbos[i]->Build();

		// Resize test	
		// TODO: automatic resize when needed
		//m_TextUbos[i]->Resize(0, sizeof(glm::mat4) * 100, Vulture::Device::GetGraphicsQueue(), Vulture::Device::GetGraphicsCommandPool());
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Create and initialize uniform buffers
		m_MainUbos.push_back(std::make_shared<Vulture::DescriptorSet>(Vulture::Renderer::GetDescriptorPool()));
		m_MainUbos[i]->AddUniformBuffer(0, sizeof(MainUbo), VK_SHADER_STAGE_VERTEX_BIT);
		m_MainUbos[i]->Build();
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_HDRDescriptorSet.push_back(std::make_shared<Vulture::DescriptorSet>(Vulture::Renderer::GetDescriptorPool()));
		m_HDRDescriptorSet[i]->AddImageSampler(0, Vulture::Renderer::GetSampler().GetSampler(), m_HDRFramebuffer[i]->GetColorImageView(0),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_SHADER_STAGE_FRAGMENT_BIT
		);
		m_HDRDescriptorSet[i]->Build();
	}

	// Create descriptor set layout for atlas
	auto atlasLayoutBuilder = Vulture::DescriptorSetLayout::Builder();
	atlasLayoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	atlasLayoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_AtlasSetLayout = std::make_shared<Vulture::DescriptorSetLayout>(atlasLayoutBuilder);

	auto fontAtlasLayoutBuilder = Vulture::DescriptorSetLayout::Builder();
	fontAtlasLayoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_FontAtlasSetLayout = std::make_shared<Vulture::DescriptorSetLayout>(fontAtlasLayoutBuilder);
}

void SceneRenderer::RecreateDescriptorSets()
{
	m_MainUbos.clear();
	m_HDRDescriptorSet.clear();
	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Create and initialize uniform buffers
		m_MainUbos.push_back(std::make_shared<Vulture::DescriptorSet>(Vulture::Renderer::GetDescriptorPool()));
		m_MainUbos[i]->AddUniformBuffer(0, sizeof(MainUbo), VK_SHADER_STAGE_VERTEX_BIT);
		m_MainUbos[i]->Build();
	}

	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_HDRDescriptorSet.push_back(std::make_shared<Vulture::DescriptorSet>(Vulture::Renderer::GetDescriptorPool()));
		m_HDRDescriptorSet[i]->AddImageSampler(0, Vulture::Renderer::GetSampler().GetSampler(), m_HDRFramebuffer[i]->GetColorImageView(0),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_SHADER_STAGE_FRAGMENT_BIT
		);
		m_HDRDescriptorSet[i]->Build();
	}
}

void SceneRenderer::CreatePipelines()
{
	// Main Geometry Pipeline

	{
		// Configure pipeline creation parameters
		Vulture::PipelineCreateInfo info{};
		info.AttributeDesc = Vulture::Mesh::Vertex::GetAttributeDescriptions();
		info.BindingDesc = Vulture::Mesh::Vertex::GetBindingDescriptions();
		info.ShaderFilepaths.push_back("src/shaders/spv/Geometry.vert.spv");
		info.ShaderFilepaths.push_back("src/shaders/spv/Geometry.frag.spv");
		info.BlendingEnable = true;
		info.DepthTestEnable = true;
		info.CullMode = VK_CULL_MODE_FRONT_BIT;
		info.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		info.Width = Vulture::Renderer::GetSwapchain().GetWidth();
		info.Height = Vulture::Renderer::GetSwapchain().GetHeight();
		info.PushConstants = nullptr;

		// Descriptor set layouts for the pipeline
		std::vector<VkDescriptorSetLayout> layouts
		{
			m_ObjectsUbos[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout(),
			m_MainUbos[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout(),
			m_AtlasSetLayout->GetDescriptorSetLayout()
		};
		info.DescriptorSetLayouts = layouts;

		m_HDRPass.CreatePipeline(info);
	}


	// Font Pipeline

	{
		VkPushConstantRange range{};
		range.offset = 0;
		range.size = sizeof(TextPushConstant);
		range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		// Configure pipeline creation parameters
		Vulture::PipelineCreateInfo info{};
		info.AttributeDesc = Vulture::Mesh::Vertex::GetAttributeDescriptions();
		info.BindingDesc = Vulture::Mesh::Vertex::GetBindingDescriptions();
		info.ShaderFilepaths.push_back("src/shaders/spv/Font.vert.spv");
		info.ShaderFilepaths.push_back("src/shaders/spv/Font.frag.spv");
		info.BlendingEnable = true;
		info.DepthTestEnable = true;
		info.CullMode = VK_CULL_MODE_BACK_BIT;
		info.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		info.Width = Vulture::Renderer::GetSwapchain().GetWidth();
		info.Height = Vulture::Renderer::GetSwapchain().GetHeight();
		info.PushConstants = &range;
		info.RenderPass = m_HDRPass.GetRenderPass();

		// Descriptor set layouts for the pipeline
		std::vector<VkDescriptorSetLayout> layouts
		{
			m_MainUbos[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout(),
			m_TextUbos[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout(),
			m_FontAtlasSetLayout->GetDescriptorSetLayout()
		};
		info.DescriptorSetLayouts = layouts;

		m_FontPipeline.CreatePipeline(info);
	}
}

void SceneRenderer::CreateFramebuffers()
{
	m_HDRFramebuffer.clear();
	std::vector<Vulture::FramebufferAttachment> attachments{ Vulture::FramebufferAttachment::ColorRGBA16, Vulture::FramebufferAttachment::Depth };
	for (int i = 0; i < Vulture::Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
	{
		m_HDRFramebuffer.push_back(std::make_unique<Vulture::Framebuffer>(attachments, m_HDRPass.GetRenderPass(), Vulture::Renderer::GetSwapchain().GetSwapchainExtent(), Vulture::Swapchain::FindDepthFormat()));
	}
}

void SceneRenderer::ImGuiPass()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Settings");

	ImGui::Text("ms %f | fps %f", m_Timer.ElapsedMillis(), 1.0f / m_Timer.ElapsedSeconds());
	m_Timer.Reset();

	ImGui::End();
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), Vulture::Renderer::GetCurrentCommandBuffer());
}
