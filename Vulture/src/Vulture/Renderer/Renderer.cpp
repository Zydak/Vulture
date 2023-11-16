#include "pch.h"
#include "Renderer.h"

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

		CreateSwapchain();
		CreateCommandBuffers();
	}

	void Renderer::Render()
	{
		VL_CORE_ASSERT(s_IsInitialized, "Renderer is not initialized");
		if (auto commandBuffer = BeginFrame())
		{
			std::vector<VkClearValue> clearColors;
			clearColors.resize(1);
			clearColors[0].color = { 0.1f, 0.1f, 0.1f, 1.0f };
			BeginRenderPass(
				commandBuffer,
				clearColors,
				s_Swapchain->GetPresentableFrameBuffer(s_CurrentImageIndex),
				s_Swapchain->GetSwapchainRenderPass(),
				glm::vec2(s_Swapchain->GetSwapchainExtent().width, s_Swapchain->GetSwapchainExtent().height)
			);

			EndRenderPass(commandBuffer);

			EndFrame();
		}
	}

	VkCommandBuffer Renderer::BeginFrame()
	{
		VL_CORE_ASSERT(!s_IsFrameStarted, "Can't call BeginFrame while already in progress!");

		auto result = s_Swapchain->AcquireNextImage(&s_CurrentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			//RecreateSwapchain();
			//CreateUniforms();

			return nullptr;
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
		return commandBuffer;
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

	void Renderer::BeginRenderPass(VkCommandBuffer commandBuffer, const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, const VkRenderPass& renderPass, glm::vec2 extent)
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Can't call BeginSwapchainRenderPass while frame is not in progress");
		VL_CORE_ASSERT(commandBuffer == GetCurrentCommandBuffer(), "Can't Begin Render pass on command buffer from different frame");

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
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = framebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = VkExtent2D{ (uint32_t)extent.x, (uint32_t)extent.y };
		renderPassInfo.clearValueCount = (uint32_t)clearColors.size();
		renderPassInfo.pClearValues = clearColors.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void Renderer::EndRenderPass(VkCommandBuffer commandBuffer)
	{
		VL_CORE_ASSERT(s_IsFrameStarted, "Can't call EndSwapchainRenderPass while frame is not in progress");
		VL_CORE_ASSERT(commandBuffer == GetCurrentCommandBuffer(), "Can't end render pass on command buffer from different frame");

		vkCmdEndRenderPass(commandBuffer);
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

	Vulture::Window* Renderer::s_Window;
	Vulture::Scope<Vulture::Swapchain> Renderer::s_Swapchain;
	std::vector<VkCommandBuffer> Renderer::s_CommandBuffers;

	bool Renderer::s_IsFrameStarted = false;
	uint32_t Renderer::s_CurrentImageIndex = 0;
	uint32_t Renderer::s_CurrentFrameIndex = 0;
	bool Renderer::s_IsInitialized = true;

}