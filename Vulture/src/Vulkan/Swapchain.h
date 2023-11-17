#pragma once
#include "pch.h"

#include "Device.h"
#include "Image.h"
#include "Framebuffer.h"
#include "RenderPass.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{

	enum class PresentModes
	{
		VSync,
		Immediate,
		MailBox
	};

	struct PresentMode
	{
		PresentModes Mode;
		bool Available;
	};

	class Swapchain
	{
	public:
		static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

		Swapchain(VkExtent2D windowExtent, const PresentModes& prefferedPresentMode);
		Swapchain(VkExtent2D windowExtent, const PresentModes& prefferedPresentMode, Ref<Swapchain> previousSwapchain);
		~Swapchain();

		Swapchain(const Swapchain&) = delete;
		Swapchain& operator=(const Swapchain&) = delete;

		inline VkRenderPass GetSwapchainRenderPass() { return m_RenderPass.GetRenderPass(); }

		VkFramebuffer GetPresentableFrameBuffer(int frameIndex);

		inline VkImageView GetPresentableImageView(int frameIndex) { return m_PresentableImageViews[frameIndex]; }

		inline uint32_t GetWidth() { return m_SwapchainExtent.width; }
		inline uint32_t GetHeight() { return m_SwapchainExtent.height; }
		inline VkFormat GetSwapchainImageFormat() { return m_SwapchainImageFormat; }
		inline uint32_t GetImageCount() { return (uint32_t)m_PresentableImageViews.size(); }
		inline VkExtent2D GetSwapchainExtent() { return m_SwapchainExtent; }

		inline std::vector<PresentMode>& GetAvailablePresentModes() { return m_AvailablePresentModes; }
		inline PresentModes GetCurrentPresentMode() { return m_CurrentPresentMode; }

		VkResult SubmitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex);
		VkResult AcquireNextImage(uint32_t* imageIndex);

		bool CompareSwapFormats(const Swapchain& swapChain) const { return swapChain.m_SwapchainDepthFormat == m_SwapchainDepthFormat && swapChain.m_SwapchainImageFormat == m_SwapchainImageFormat; }

		float GetExtentAspectRatio() { return float(m_SwapchainExtent.width) / float(m_SwapchainExtent.height); }

		static VkFormat FindDepthFormat();
	private:
		void CreateSwapchain(const PresentModes& prefferedPresentMode);
		void CreateImageViews();
		void CreateRenderPass();
		void CreateFramebuffers();
		void CreateSyncObjects();

		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
		VkPresentModeKHR ChooseSwapPresentMode(const PresentModes& presentMode);
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
		void FindPresentModes(const std::vector<VkPresentModeKHR>& availablePresentModes);
		std::vector<PresentMode> m_AvailablePresentModes;
		PresentModes m_CurrentPresentMode;

		Ref<Swapchain> m_OldSwapchain;
		VkSwapchainKHR m_Swapchain;
		VkExtent2D m_WindowExtent;

		uint32_t m_CurrentFrame = 0;
		std::vector<VkFramebuffer> m_PresentableFramebuffers;

		std::vector<VkImage> m_PresentableImages;
		std::vector<VkImageView> m_PresentableImageViews;

		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence> m_InFlightFences;
		std::vector<VkFence> m_ImagesInFlight;

		VkFormat m_SwapchainImageFormat;
		VkFormat m_SwapchainDepthFormat;
		VkExtent2D m_SwapchainExtent;

		RenderPass m_RenderPass{};
	};

}