#pragma once
#include "pch.h"
#include "Utility/Utility.h"

#include "Vulkan/Swapchain.h"
#include "Vulkan/Descriptors.h"

#include <vulkan/vulkan.h>

namespace Vulture
{
	class Renderer
	{
	public:
		~Renderer() = delete;
		Renderer() = delete;

		static void Init(Window& window);
		static void Destroy();
		static void Render();
	private:
		static VkCommandBuffer BeginFrame();
		static void EndFrame();
		static void BeginRenderPass(VkCommandBuffer commandBuffer, const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, const VkRenderPass& renderPass, glm::vec2 extent);
		static void EndRenderPass(VkCommandBuffer commandBuffer);

		static void CreateSwapchain();
		static void CreateCommandBuffers();

		static VkCommandBuffer GetCurrentCommandBuffer();
		static int GetFrameIndex();

		static Window* s_Window;
		static std::vector<VkCommandBuffer> s_CommandBuffers;
		static Scope<Swapchain> s_Swapchain;

		static bool s_IsFrameStarted;
		static uint32_t s_CurrentImageIndex;
		static uint32_t s_CurrentFrameIndex;

		static bool s_IsInitialized;
	};
}