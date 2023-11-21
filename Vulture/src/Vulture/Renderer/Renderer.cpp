#include "pch.h"
#include "Renderer.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"

namespace Vulture
{
	void Renderer::ImageMemoryBarrier(VkImage image, VkCommandBuffer commandBuffer, VkImageAspectFlagBits aspect,
		VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount, uint32_t baseLayer)
	{
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;

		barrier.subresourceRange.aspectMask = aspect;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = baseLayer;
		barrier.subresourceRange.layerCount = layerCount;

		barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		// Add a dependency on the fragment shader stage
		vkCmdPipelineBarrier(
			commandBuffer,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}

	void Renderer::Destroy()
	{
		s_IsInitialized = false;
		vkDeviceWaitIdle(Device::GetDevice());
		vkFreeCommandBuffers(Device::GetDevice(), Device::GetCommandPool(), (uint32_t)s_CommandBuffers.size(), s_CommandBuffers.data());
		
		s_Swapchain.release();
	}

	void Renderer::Init(Window& window)
	{
		s_RendererSampler = std::make_unique<Sampler>(SamplerInfo(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR));
		
		s_IsInitialized = true;
		s_Window = &window;
		s_QuadMesh.Init();
		CreatePool();
		CreateRenderPass();
		RecreateSwapchain();
		CreateCommandBuffers();
	}

	void Renderer::Render(Scene& scene)
	{
		s_CurrentSceneRendered = &scene;
		VL_CORE_ASSERT(s_IsInitialized, "Renderer is not initialized");

		if (BeginFrame())
		{
			UpdateStorageBuffer();

			GeometryPass();
			PostProcessPass();

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
		std::vector<VkClearValue> clearColors;
		clearColors.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
		VkClearValue clearVal;
		clearVal.depthStencil = { 1.0f, 1 };
		clearColors.push_back(clearVal);
		// Begin the render pass
		BeginRenderPass(
			clearColors,
			s_HDRFramebuffer[s_CurrentFrameIndex]->GetFramebuffer(),
			s_HDRRenderPass.GetRenderPass(),
			s_HDRFramebuffer[s_CurrentFrameIndex]->GetExtent()
		);
		// Bind the geometry pipeline
		s_GeometryPipeline.Bind(GetCurrentCommandBuffer());

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

	void Renderer::PostProcessPass()
	{
		std::vector<VkClearValue> clearColors;
		clearColors.push_back({ 0.0f, 0.0f, 0.0f, 0.0f });
		// Begin the render pass
		BeginRenderPass(
			clearColors,
			s_Swapchain->GetPresentableFrameBuffer(s_CurrentImageIndex),
			s_Swapchain->GetSwapchainRenderPass(),
			glm::vec2(s_Swapchain->GetSwapchainExtent().width, s_Swapchain->GetSwapchainExtent().height)
		);
		// Bind the geometry pipeline
		s_HDRToPresentablePipeline.Bind(GetCurrentCommandBuffer());

		s_HDRUniforms[s_CurrentFrameIndex]->Bind(
			0,
			s_HDRToPresentablePipeline.GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			GetCurrentCommandBuffer()
		);

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

		// get camera matrix of the main camera
		auto cameraView = s_CurrentSceneRendered->GetRegistry().view<CameraComponent>();
		MainUbo mainUbo;
		for (auto cameraEntity : cameraView)
		{
			auto& cameraComponent = cameraView.get<CameraComponent>(cameraEntity);
			if (cameraComponent.Main)
			{
				mainUbo.ViewProjMatrix = cameraComponent.GetViewProj();
				break;
			}
		}

		// Write the camera matrix data to the UBO buffer and flush it
		s_ObjectsUbos[s_CurrentFrameIndex]->GetBuffer(1)->WriteToBuffer(&mainUbo, sizeof(MainUbo), 0);
		s_ObjectsUbos[s_CurrentFrameIndex]->GetBuffer(1)->Flush();
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
		CreateFramebuffer();
		CreateUniforms();
		CreatePipeline();
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
		s_HDRUniforms.clear();

		for (int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			// Create and initialize uniform buffers
			s_ObjectsUbos.push_back(std::make_shared<Uniform>(*s_Pool));
			s_ObjectsUbos[i]->AddStorageBuffer(0, sizeof(StorageBufferEntry), VK_SHADER_STAGE_VERTEX_BIT, true);
			s_ObjectsUbos[i]->AddUniformBuffer(1, sizeof(MainUbo), VK_SHADER_STAGE_VERTEX_BIT);
			s_ObjectsUbos[i]->Build();

			// Resize test
			s_ObjectsUbos[i]->Resize(0, sizeof(StorageBufferEntry) * 10, Device::GetGraphicsQueue(), Device::GetCommandPool());
		}

		for (int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			s_HDRUniforms.push_back(std::make_shared<Uniform>(*s_Pool));
			s_HDRUniforms[i]->AddImageSampler(0, s_RendererSampler->GetSampler(), s_HDRFramebuffer[i]->GetColorImageView(0),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_SHADER_STAGE_FRAGMENT_BIT
			);
			s_HDRUniforms[i]->Build();
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
		// 
		// Geometry
		// 
		
		{
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
			info.PushConstants = nullptr;
			info.RenderPass = s_HDRRenderPass.GetRenderPass();

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

		//
		// Geometry To HDR
		//

		{
			PipelineCreateInfo info{};
			info.AttributeDesc = Quad::Vertex::GetAttributeDescriptions();
			info.BindingDesc = Quad::Vertex::GetBindingDescriptions();
			info.ShaderFilepaths.push_back("../Vulture/src/Vulture/Shaders/spv/HDRToPresentable.vert.spv");
			info.ShaderFilepaths.push_back("../Vulture/src/Vulture/Shaders/spv/HDRToPresentable.frag.spv");
			info.BlendingEnable = false;
			info.DepthTestEnable = false;
			info.CullMode = VK_CULL_MODE_BACK_BIT;
			info.Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			info.Width = s_Swapchain->GetWidth();
			info.Height = s_Swapchain->GetHeight();
			info.PushConstants = nullptr;
			info.RenderPass = s_Swapchain->GetSwapchainRenderPass();

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				s_HDRUniforms[0]->GetDescriptorSetLayout()->GetDescriptorSetLayout()
			};
			info.UniformSetLayouts = layouts;

			// Create the graphics pipeline
			s_HDRToPresentablePipeline.CreatePipeline(info);
		}
	}

	void Renderer::CreateFramebuffer()
	{
		std::vector<FramebufferAttachment> attachments{ FramebufferAttachment::ColorRGBA16, FramebufferAttachment::Depth };
		for (int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++)
		{
			s_HDRFramebuffer.push_back(std::make_unique<Framebuffer>(attachments, s_HDRRenderPass.GetRenderPass(), s_Swapchain->GetSwapchainExtent(), Swapchain::FindDepthFormat()));
		}
	}

	void Renderer::CreateRenderPass()
	{
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
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
		depthAttachment.format = Swapchain::FindDepthFormat();
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

		s_HDRRenderPass.CreateRenderPass(renderPassInfo);
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
	std::vector<StorageBufferEntry> Renderer::s_StorageBuffer;
	Pipeline Renderer::s_GeometryPipeline;
	Pipeline Renderer::s_HDRToPresentablePipeline;
	std::vector<std::shared_ptr<Uniform>> Renderer::s_ObjectsUbos;
	std::shared_ptr<DescriptorSetLayout> Renderer::s_AtlasSetLayout;
	Quad Renderer::s_QuadMesh;
	Scope<Sampler> Renderer::s_RendererSampler;
	RenderPass Renderer::s_HDRRenderPass;
	std::vector<Scope<Framebuffer>> Renderer::s_HDRFramebuffer;
	std::vector<Ref<Uniform>> Renderer::s_HDRUniforms;
}