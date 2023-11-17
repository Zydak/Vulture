#include "pch.h"
#include "Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"

namespace Vulture
{
	void Renderer::Destroy()
	{
		s_IsInitialized = false;
		vkDeviceWaitIdle(Device::GetDevice());
		vkFreeCommandBuffers(Device::GetDevice(), Device::GetCommandPool(), (uint32_t)s_CommandBuffers.size(), s_CommandBuffers.data());
		
		s_Swapchain.release();
	}

	void Renderer::Init(Window& window)
	{
		s_IsInitialized = true;
		s_Window = &window;
		s_QuadMesh.Init();
		CreatePool();
		CreateSwapchain();
		CreateCommandBuffers();
		CreateUniforms();
		CreatePipeline();
	}

	void Renderer::Render(Scene& scene)
	{
		s_CurrentSceneRendered = &scene;
		VL_CORE_ASSERT(s_IsInitialized, "Renderer is not initialized");

		if (BeginFrame())
		{
			UpdateStorageBuffer();

			GeometryPass();

			EndFrame();
		}
	}

	bool Renderer::BeginFrame()
	{
		VL_CORE_ASSERT(!s_IsFrameStarted, "Can't call BeginFrame while already in progress!");

		auto result = s_Swapchain->AcquireNextImage(&s_CurrentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			//RecreateSwapchain();
			//CreateUniforms();

			return false;
		}
		VL_CORE_ASSERT((result == VK_SUCCESS && result != VK_SUBOPTIMAL_KHR), "failed to acquire swap chain image!");

		s_IsFrameStarted = true;
		auto commandBuffer = GetCurrentCommandBuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		VL_CORE_RETURN_ASSERT(
			vkBeginCommandBuffer(commandBuffer, &beginInfo),
			VK_SUCCESS,
			"failed to begin recording command buffer!"
		);
		return true;
	}

	void Renderer::EndFrame()
	{
		auto commandBuffer = GetCurrentCommandBuffer();
		VL_CORE_ASSERT(s_IsFrameStarted, "Can't call EndFrame while frame is not in progress");

		auto success = vkEndCommandBuffer(commandBuffer);
		VL_CORE_ASSERT(success == VK_SUCCESS, "failed to record command buffer!");

		auto result = s_Swapchain->SubmitCommandBuffers(&commandBuffer, &s_CurrentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || s_Window->WasWindowResized())
		{
			s_Window->ResetWindowResizedFlag();
			//RecreateSwapchain();
			//CreateUniforms();
		}
		else 
			VL_CORE_ASSERT(result == VK_SUCCESS, "failed to present swap chain image!");

		s_IsFrameStarted = false;
		s_CurrentFrameIndex = (s_CurrentFrameIndex + 1) % Swapchain::MAX_FRAMES_IN_FLIGHT;
	}

	void Renderer::BeginRenderPass(const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, const VkRenderPass& renderPass, glm::vec2 extent)
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Can't call BeginSwapchainRenderPass while frame is not in progress");

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = extent.x;
		viewport.height = extent.y;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		VkRect2D scissor{
			{0, 0},
			VkExtent2D{(uint32_t)extent.x, (uint32_t)extent.y}
		};
		vkCmdSetViewport(GetCurrentCommandBuffer(), 0, 1, &viewport);
		vkCmdSetScissor(GetCurrentCommandBuffer(), 0, 1, &scissor);

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = framebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = VkExtent2D{ (uint32_t)extent.x, (uint32_t)extent.y };
		renderPassInfo.clearValueCount = (uint32_t)clearColors.size();
		renderPassInfo.pClearValues = clearColors.data();

		vkCmdBeginRenderPass(GetCurrentCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void Renderer::EndRenderPass()
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Can't call EndSwapchainRenderPass while frame is not in progress");
		VL_CORE_ASSERT(GetCurrentCommandBuffer() == GetCurrentCommandBuffer(), "Can't end render pass on command buffer from different frame");

		vkCmdEndRenderPass(GetCurrentCommandBuffer());
	}

	void Renderer::GeometryPass()
	{
		std::vector<VkClearValue> clearColors;
		clearColors.resize(1);
		clearColors[0].color = { 0.1f, 0.1f, 0.1f, 1.0f };
		BeginRenderPass(
			clearColors,
			s_Swapchain->GetPresentableFrameBuffer(s_CurrentImageIndex),
			s_Swapchain->GetSwapchainRenderPass(),
			glm::vec2(s_Swapchain->GetSwapchainExtent().width, s_Swapchain->GetSwapchainExtent().height)
		);

		s_GeometryPipeline.Bind(GetCurrentCommandBuffer());

		glm::mat4 push{ 1.0f };

		vkCmdPushConstants(
			GetCurrentCommandBuffer(),
			s_GeometryPipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(glm::mat4),
			&push
		);

		s_ObjectsUbos[s_CurrentFrameIndex]->Bind(
			0,
			s_GeometryPipeline.GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			GetCurrentCommandBuffer()
		);

		s_CurrentSceneRendered->GetAtlas().GetAtlasUniform()->Bind(
			1,
			s_GeometryPipeline.GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			GetCurrentCommandBuffer()
		);

		s_QuadMesh.Bind(GetCurrentCommandBuffer());
		s_QuadMesh.Draw(GetCurrentCommandBuffer(), (uint32_t)s_StorageBuffer.size());

		EndRenderPass();
	}

	void Renderer::UpdateStorageBuffer()
	{
		auto view = s_CurrentSceneRendered->GetRegistry().view<TransformComponent, SpriteComponent>();
		s_StorageBuffer.clear();
		for (auto entity : view)
		{
			const auto& [transformComponent, sprite] = view.get<TransformComponent, SpriteComponent>(entity);

			StorageBufferEntry entry;
			entry.ModelMatrix = transformComponent.transform.GetMat4();
			entry.AtlasOffset = glm::vec4(sprite.AtlasOffsets, 1.0f, 1.0f);

			s_StorageBuffer.push_back(entry);
		}

		s_ObjectsUbos[s_CurrentFrameIndex]->GetBuffer(0)->WriteToBuffer(s_StorageBuffer.data(), s_StorageBuffer.size() * sizeof(StorageBufferEntry), 0);
		s_ObjectsUbos[s_CurrentFrameIndex]->GetBuffer(0)->Flush();
	}

	void Renderer::CreateSwapchain()
	{
		auto extent = s_Window->GetExtent();
		while (extent.width == 0 || extent.height == 0)
		{
			extent = s_Window->GetExtent();
			glfwWaitEvents();
		}
		vkDeviceWaitIdle(Device::GetDevice());

		s_Swapchain = std::make_unique<Swapchain>(extent, PresentModes::VSync);

		//CreateRenderPasses();
		//CreateFramebuffers();
		//CreatePipelines();
	}

	void Renderer::CreateCommandBuffers()
	{
		s_CommandBuffers.resize(s_Swapchain->GetImageCount());

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = Device::GetCommandPool();
		allocInfo.commandBufferCount = (uint32_t)s_CommandBuffers.size();
		VL_CORE_RETURN_ASSERT(
			vkAllocateCommandBuffers(Device::GetDevice(), &allocInfo, s_CommandBuffers.data()),
			VK_SUCCESS,
			"failed to allocate command buffers!"
		);
	}

	void Renderer::CreateUniforms()
	{
		for (int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			s_ObjectsUbos.push_back(std::make_shared<Uniform>(*s_Pool));
			s_ObjectsUbos[i]->AddStorageBuffer(0, sizeof(StorageBufferEntry), VK_SHADER_STAGE_VERTEX_BIT, true);
			s_ObjectsUbos[i]->Build();
			// Resize test
			s_ObjectsUbos[i]->Resize(0, sizeof(StorageBufferEntry) * 10, Device::GetGraphicsQueue(), Device::GetCommandPool());
		}

		auto atlasLayoutBuilder = DescriptorSetLayout::Builder();
		atlasLayoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		atlasLayoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		s_AtlasSetLayout = std::make_shared<DescriptorSetLayout>(atlasLayoutBuilder);
	}

	void Renderer::CreatePool()
	{
		auto poolBuilder = DescriptorPool::Builder();
		poolBuilder.SetMaxSets((Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);
		poolBuilder.SetPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
		poolBuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);
		poolBuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);
		poolBuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);
		poolBuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);
		
		s_Pool = std::make_unique<DescriptorPool>(poolBuilder);
	}

	void Renderer::CreatePipeline()
	{
		VkPushConstantRange range;
		range.offset = 0;
		range.size = sizeof(glm::mat4);
		range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		PipelineCreateInfo info{};
		info.AttributeDesc = Quad::Vertex::GetAttributeDescriptions();
		info.BindingDesc = Quad::Vertex::GetBindingDescriptions();
		info.ShaderFilepaths.push_back("../Vulture/src/Vulture/Shaders/spv/Geometry.vert.spv");
		info.ShaderFilepaths.push_back("../Vulture/src/Vulture/Shaders/spv/Geometry.frag.spv");
		info.BlendingEnable = false;
		info.DepthTestEnable = false;
		info.CullMode = VK_CULL_MODE_NONE;
		info.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		info.Width = s_Swapchain->GetWidth();
		info.Height = s_Swapchain->GetHeight();
		info.PushConstants = &range;
		info.RenderPass = s_Swapchain->GetSwapchainRenderPass();
		std::vector<VkDescriptorSetLayout> layouts
		{
			s_ObjectsUbos[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout(),
			s_AtlasSetLayout->GetDescriptorSetLayout()
		};
		info.UniformSetLayouts = layouts;
		s_GeometryPipeline.CreatePipeline(info);
	}

	VkCommandBuffer Renderer::GetCurrentCommandBuffer()
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Cannot get command buffer when frame is not in progress");
		return s_CommandBuffers[s_CurrentImageIndex];
	}

	int Renderer::GetFrameIndex()
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Cannot get frame index when frame is not in progress");
		return s_CurrentFrameIndex;
	}

	Window* Renderer::s_Window;
	Scope<DescriptorPool> Renderer::s_Pool;
	Scope<Swapchain> Renderer::s_Swapchain;
	std::vector<VkCommandBuffer> Renderer::s_CommandBuffers;
	bool Renderer::s_IsFrameStarted = false;
	uint32_t Renderer::s_CurrentImageIndex = 0;
	uint32_t Renderer::s_CurrentFrameIndex = 0;
	Scene* Renderer::s_CurrentSceneRendered;
	bool Renderer::s_IsInitialized = true;
	std::vector<Vulture::StorageBufferEntry> Renderer::s_StorageBuffer;
	Pipeline Renderer::s_GeometryPipeline;
	std::vector<std::shared_ptr<Uniform>> Renderer::s_ObjectsUbos;
	std::shared_ptr<Vulture::DescriptorSetLayout> Renderer::s_AtlasSetLayout;
	Vulture::Quad Renderer::s_QuadMesh;

}