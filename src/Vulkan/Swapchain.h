// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "pch.h"

#include "Device.h"
#include "Image.h"
#include "Framebuffer.h"

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

		struct CreateInfo
		{
			VkExtent2D WindowExtent = { 0, 0 };
			PresentModes PrefferedPresentMode = PresentModes::VSync;
			uint32_t MaxFramesInFlight = 0;
			Ref<Swapchain> PreviousSwapchain = nullptr;
		};

		void Init(const CreateInfo& createInfo);
		void Destroy();

		Swapchain() = default;
		Swapchain(const CreateInfo& createInfo);
		~Swapchain();

		Swapchain(const Swapchain&) = delete;
		Swapchain& operator=(const Swapchain&) = delete;
		Swapchain(Swapchain&& other) noexcept;
		Swapchain& operator=(Swapchain&& other) noexcept;

		inline VkRenderPass GetSwapchainRenderPass() const { return m_RenderPass; }

		VkFramebuffer GetPresentableFrameBuffer(int frameIndex) const { return m_PresentableFramebuffers[frameIndex]; };

		inline VkImageView GetPresentableImageView(int frameIndex) const { return m_PresentableImageViews[frameIndex]; }
		inline VkImage GetPresentableImage(int frameIndex) const { return m_PresentableImages[frameIndex]; }

		inline uint32_t GetWidth() const { return m_SwapchainExtent.width; }
		inline uint32_t GetHeight() const { return m_SwapchainExtent.height; }
		inline VkFormat GetSwapchainImageFormat() const { return m_SwapchainImageFormat; }
		inline uint32_t GetImageCount() const { return (uint32_t)m_PresentableImageViews.size(); }
		inline VkExtent2D GetSwapchainExtent() const { return m_SwapchainExtent; }

		inline bool IsInitialized() const { return m_Initialized; }

		inline const std::vector<PresentMode>& GetAvailablePresentModes() const { return m_AvailablePresentModes; }
		inline PresentModes GetCurrentPresentMode() const { return m_CurrentPresentMode; }

		float GetExtentAspectRatio() const { return float(m_SwapchainExtent.width) / float(m_SwapchainExtent.height); }

		VkResult SubmitCommandBuffers(const VkCommandBuffer* buffers, uint32_t& imageIndex);
		VkResult AcquireNextImage(uint32_t& imageIndex);

		bool CompareSwapFormats(const Swapchain& swapChain) const { return swapChain.m_SwapchainDepthFormat == m_SwapchainDepthFormat && swapChain.m_SwapchainImageFormat == m_SwapchainImageFormat; }

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

	private:
		std::vector<PresentMode> m_AvailablePresentModes;
		PresentModes m_CurrentPresentMode;

		uint32_t m_MaxFramesInFlight;

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

		VkRenderPass m_RenderPass{};

		bool m_Initialized = false;

		void Reset();
	};

}