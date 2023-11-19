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
		RecreateSwapchain();
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

	/*
	 * @brief Acquires the next swap chain image and begins recording a command buffer for rendering. 
	 * If the swap chain is out of date, it may trigger swap chain recreation.
	 *
	 * @return True if the frame started successfully; false if the swap chain needs recreation.
	 */
	bool Renderer::BeginFrame()
	{
		VL_CORE_ASSERT(!s_IsFrameStarted, "Can't call BeginFrame while already in progress!");

		auto result = s_Swapchain->AcquireNextImage(s_CurrentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			RecreateSwapchain();

			return false;
		}
		VL_CORE_ASSERT((result == VK_SUCCESS && result != VK_SUBOPTIMAL_KHR), "failed to acquire swap chain image!");

		s_IsFrameStarted = true;
		auto commandBuffer = GetCurrentCommandBuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		VL_CORE_RETURN_ASSERT(
			vkBeginCommandBuffer(commandBuffer, &beginInfo),
			VK_SUCCESS,
			"failed to begin recording command buffer!"
		);
		return true;
	}

	/*
	 * @brief Finalizes the recorded command buffer, submits it for execution, and presents the swap chain image. 
	 * If the swap chain is out of date, it may trigger swap chain recreation.
	 */
	void Renderer::EndFrame()
	{
		auto commandBuffer = GetCurrentCommandBuffer();
		VL_CORE_ASSERT(s_IsFrameStarted, "Cannot call EndFrame while frame is not in progress");

		// End recording the command buffer
		auto success = vkEndCommandBuffer(commandBuffer);
		VL_CORE_ASSERT(success == VK_SUCCESS, "Failed to record command buffer!");

		// Submit the command buffer for execution and present the image
		auto result = s_Swapchain->SubmitCommandBuffers(&commandBuffer, s_CurrentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || s_Window->WasWindowResized())
		{
			// Recreate swap chain or handle window resize
			s_Window->ResetWindowResizedFlag();
			RecreateSwapchain();
		}
		else
		{
			VL_CORE_ASSERT(result == VK_SUCCESS, "Failed to present swap chain image!");
		}

		// End the frame and update frame index
		s_IsFrameStarted = false;
		s_CurrentFrameIndex = (s_CurrentFrameIndex + 1) % Swapchain::MAX_FRAMES_IN_FLIGHT;
	}

	/*
	 * @brief Sets up the rendering viewport, scissor, and begins the specified render pass on the given framebuffer. 
	 * It also clears the specified colors in the render pass.
	 *
	 * @param clearColors - A vector of clear values for the attachments in the render pass.
	 * @param framebuffer - The framebuffer to use in the render pass.
	 * @param renderPass - The render pass to begin.
	 * @param extent - The extent (width and height) of the render area.
	 */
	void Renderer::BeginRenderPass(const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, const VkRenderPass& renderPass, glm::vec2 extent)
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Cannot call BeginSwapchainRenderPass while frame is not in progress");

		// Set up viewport
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = extent.x;
		viewport.height = extent.y;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// Set up scissor
		VkRect2D scissor{
			{0, 0},
			VkExtent2D{(uint32_t)extent.x, (uint32_t)extent.y}
		};

		// Set viewport and scissor for the current command buffer
		vkCmdSetViewport(GetCurrentCommandBuffer(), 0, 1, &viewport);
		vkCmdSetScissor(GetCurrentCommandBuffer(), 0, 1, &scissor);

		// Set up render pass information
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = framebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = VkExtent2D{ (uint32_t)extent.x, (uint32_t)extent.y };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
		renderPassInfo.pClearValues = clearColors.data();

		// Begin the render pass for the current command buffer
		vkCmdBeginRenderPass(GetCurrentCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	/**
	 * @brief Ends the current render pass in progress. 
	 * It should be called after rendering commands within a render pass have been recorded.
	 */
	void Renderer::EndRenderPass()
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Can't call EndSwapchainRenderPass while frame is not in progress");
		VL_CORE_ASSERT(GetCurrentCommandBuffer() == GetCurrentCommandBuffer(), "Can't end render pass on command buffer from different frame");

		vkCmdEndRenderPass(GetCurrentCommandBuffer());
	}

	/*
	 * @brief Renders geometry.
	 */
	void Renderer::GeometryPass()
	{
		// Set up clear colors for the render pass
		std::vector<VkClearValue> clearColors(1, { 0.0f, 0.0f, 0.0f, 0.0f });

		// Begin the render pass
		BeginRenderPass(
			clearColors,
			s_Swapchain->GetPresentableFrameBuffer(s_CurrentImageIndex),
			s_Swapchain->GetSwapchainRenderPass(),
			glm::vec2(s_Swapchain->GetSwapchainExtent().width, s_Swapchain->GetSwapchainExtent().height)
		);

		// Bind the geometry pipeline
		s_GeometryPipeline.Bind(GetCurrentCommandBuffer());

		// Set up push constants
		glm::mat4 push{ 1.0f };
		vkCmdPushConstants(
			GetCurrentCommandBuffer(),
			s_GeometryPipeline.GetPipelineLayout(),
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(glm::mat4),
			&push
		);

		// Bind UBOs and descriptor sets
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

		// Bind and draw the quad mesh
		s_QuadMesh.Bind(GetCurrentCommandBuffer());
		s_QuadMesh.Draw(GetCurrentCommandBuffer(), static_cast<uint32_t>(s_StorageBuffer.size()));

		// End the render pass
		EndRenderPass();
	}

	void Renderer::UpdateStorageBuffer()
	{
		// Retrieve entities with TransformComponent and SpriteComponent from the current scene
		auto view = s_CurrentSceneRendered->GetRegistry().view<TransformComponent, SpriteComponent>();

		// Clear the storage buffer
		s_StorageBuffer.clear();

		// Iterate over entities and update storage buffer entries
		for (auto entity : view)
		{
			const auto& [transformComponent, sprite] = view.get<TransformComponent, SpriteComponent>(entity);

			// Create a storage buffer entry for the entity
			StorageBufferEntry entry;
			entry.ModelMatrix = transformComponent.transform.GetMat4();
			entry.AtlasOffset = glm::vec4(sprite.AtlasOffsets, 1.0f, 1.0f);

			// Add the entry to the storage buffer
			s_StorageBuffer.push_back(entry);
		}

		// Write the storage buffer data to the UBO buffer and flush it
		s_ObjectsUbos[s_CurrentFrameIndex]->GetBuffer(0)->WriteToBuffer(s_StorageBuffer.data(), s_StorageBuffer.size() * sizeof(StorageBufferEntry), 0);
		s_ObjectsUbos[s_CurrentFrameIndex]->GetBuffer(0)->Flush();
	}

	/*
	 * @brief This function is called when the window is resized or the swapchain needs to be recreated for any reason.
	 */
	void Renderer::RecreateSwapchain()
	{
		// Wait for the window to have a valid extent
		auto extent = s_Window->GetExtent();
		while (extent.width == 0 || extent.height == 0)
		{
			extent = s_Window->GetExtent();
			glfwWaitEvents();
		}

		// Wait for the device to be idle before recreating the swapchain
		vkDeviceWaitIdle(Device::GetDevice());

		// Recreate the swapchain
		if (s_Swapchain == nullptr)
		{
			s_Swapchain = std::make_unique<Swapchain>(extent, PresentModes::VSync);
		}
		else
		{
			// Move the old swapchain into a shared pointer to ensure it is properly cleaned up
			std::shared_ptr<Swapchain> oldSwapchain = std::move(s_Swapchain);

			// Create a new swapchain using the old one as a reference
			s_Swapchain = std::make_unique<Swapchain>(extent, PresentModes::VSync, oldSwapchain);

			// Check if the swap formats are consistent
			VL_CORE_ASSERT(oldSwapchain->CompareSwapFormats(*s_Swapchain), "Swap chain image or depth formats have changed!");
		}

		// Recreate other resources dependent on the swapchain, such as uniforms, framebuffers and pipelines
		CreateUniforms();
		CreatePipeline();
		//CreateFramebuffers();
	}

	/*
	 * @brief Allocates primary command buffers from the command pool for each swap chain image.
	 */
	void Renderer::CreateCommandBuffers()
	{
		// Resize the command buffer array to match the number of swap chain images
		s_CommandBuffers.resize(s_Swapchain->GetImageCount());

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = Device::GetCommandPool();
		allocInfo.commandBufferCount = (uint32_t)s_CommandBuffers.size();

		// Allocate primary command buffers
		VL_CORE_RETURN_ASSERT(
			vkAllocateCommandBuffers(Device::GetDevice(), &allocInfo, s_CommandBuffers.data()),
			VK_SUCCESS,
			"Failed to allocate command buffers!"
		);
	}

	/*
	 * @brief Creates and initializes uniform buffers and descriptor set layouts.
	 */
	void Renderer::CreateUniforms()
	{
		s_ObjectsUbos.clear();
		for (int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			// Create and initialize uniform buffers
			s_ObjectsUbos.push_back(std::make_shared<Uniform>(*s_Pool));
			s_ObjectsUbos[i]->AddStorageBuffer(0, sizeof(StorageBufferEntry), VK_SHADER_STAGE_VERTEX_BIT, true);
			s_ObjectsUbos[i]->Build();

			// Resize test
			s_ObjectsUbos[i]->Resize(0, sizeof(StorageBufferEntry) * 10, Device::GetGraphicsQueue(), Device::GetCommandPool());
		}

		// Create descriptor set layout for atlas
		auto atlasLayoutBuilder = DescriptorSetLayout::Builder();
		atlasLayoutBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		atlasLayoutBuilder.AddBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
		s_AtlasSetLayout = std::make_shared<DescriptorSetLayout>(atlasLayoutBuilder);
	}

	/*
	 * @brief Creates the descriptor pool for managing descriptor sets.
	 */
	void Renderer::CreatePool()
	{
		auto poolBuilder = DescriptorPool::Builder();
		poolBuilder.SetMaxSets((Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);
		poolBuilder.SetPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
		poolBuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);
		poolBuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);
		poolBuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);
		poolBuilder.AddPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (Swapchain::MAX_FRAMES_IN_FLIGHT) * 1000);

		// Create and initialize the descriptor pool
		s_Pool = std::make_unique<DescriptorPool>(poolBuilder);
	}

	/**
	 * @brief Creates the graphics pipeline for rendering geometry.
	 */
	void Renderer::CreatePipeline()
	{
		// Push constant range for the vertex shader
		VkPushConstantRange range;
		range.offset = 0;
		range.size = sizeof(glm::mat4);
		range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		// Configure pipeline creation parameters
		PipelineCreateInfo info{};
		info.AttributeDesc = Quad::Vertex::GetAttributeDescriptions();
		info.BindingDesc = Quad::Vertex::GetBindingDescriptions();
		info.ShaderFilepaths.push_back("../Vulture/src/Vulture/Shaders/spv/Geometry.vert.spv");
		info.ShaderFilepaths.push_back("../Vulture/src/Vulture/Shaders/spv/Geometry.frag.spv");
		info.BlendingEnable = true;
		info.DepthTestEnable = true;
		info.CullMode = VK_CULL_MODE_BACK_BIT;
		info.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		info.Width = s_Swapchain->GetWidth();
		info.Height = s_Swapchain->GetHeight();
		info.PushConstants = &range;
		info.RenderPass = s_Swapchain->GetSwapchainRenderPass();

		// Descriptor set layouts for the pipeline
		std::vector<VkDescriptorSetLayout> layouts
		{
			s_ObjectsUbos[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout(),
			s_AtlasSetLayout->GetDescriptorSetLayout()
		};
		info.UniformSetLayouts = layouts;

		// Create the graphics pipeline
		s_GeometryPipeline.CreatePipeline(info);
	}

	/**
	 * @brief Retrieves the current command buffer for rendering.
	 *
	 * @return The current command buffer.
	 */
	VkCommandBuffer Renderer::GetCurrentCommandBuffer()
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Cannot get command buffer when frame is not in progress");
		return s_CommandBuffers[s_CurrentImageIndex];
	}

	/**
	 * @brief Retrieves the index of the current frame in progress.
	 *
	 * @return The index of the current frame.
	 */
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