#pragma once
#include "pch.h"

#include "Window.h"
#include <vulkan/vulkan.h>

namespace Vulture
{

	struct SwapchainSupportDetails
	{
		VkSurfaceCapabilitiesKHR Capabilities;      // min/max number of images
		std::vector<VkSurfaceFormatKHR> Formats;    // pixel format, color space
		std::vector<VkPresentModeKHR> PresentModes;
	};

	struct QueueFamilyIndices
	{
		uint32_t GraphicsFamily;
		uint32_t PresentFamily;
		bool GraphicsFamilyHasValue = false;
		bool PresentFamilyHasValue = false;

		bool IsComplete() { return GraphicsFamilyHasValue && PresentFamilyHasValue; }
	};

	struct Extension
	{
		const char* Name;
		bool supported;
	};

	class Device
	{
	public:
		static void Init(Window& window);
		~Device();

		Device(const Device&) = delete;
		Device& operator=(const Device&) = delete;
		Device(Device&&) = delete;
		Device& operator=(Device&&) = delete;

		static inline VkInstance GetInstance() { return s_Instance; }
		static inline VkDevice GetDevice() { return s_Device; }
		static inline VkPhysicalDevice GetPhysicalDevice() { return s_PhysicalDevice; }
		static inline SwapchainSupportDetails GetSwapchainSupport() { return QuerySwapchainSupport(s_PhysicalDevice); }
		static inline VkSurfaceKHR GetSurface() { return s_Surface; }
		static inline QueueFamilyIndices FindPhysicalQueueFamilies() { return FindQueueFamilies(s_PhysicalDevice); }
		static inline VkCommandPool GetCommandPool() { return s_CommandPool; }
		static inline VkQueue GetGraphicsQueue() { return s_GraphicsQueue; }
		static inline VkQueue GetPresentQueue() { return s_PresentQueue; }

		static inline VkPhysicalDeviceProperties GetDeviceProperties() { return s_Properties; }

		static VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		
		static void EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool);
		static void BeginSingleTimeCommands(VkCommandBuffer& buffer, VkCommandPool pool);
		static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	private:
		Device() {} // make constructor private

		static std::vector<const char*> GetRequiredGlfwExtensions();
		static void CheckRequiredGlfwExtensions();
		static bool IsDeviceSuitable(VkPhysicalDevice device);
		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

		static void CreateInstance();
		static void SetupDebugMessenger();
		static void CreateSurface();
		static void PickPhysicalDevice();
		static void CreateLogicalDevice();
		static void CreateCommandPool();

		static void PopulateDebugMessenger(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		static bool CheckValidationLayerSupport();
		static bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
		static SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device);

		static VkPhysicalDeviceProperties s_Properties;
		static VkPhysicalDeviceFeatures s_Features;
		static VkInstance s_Instance;
		static VkDebugUtilsMessengerEXT s_DebugMessenger;
		static VkPhysicalDevice s_PhysicalDevice;
		static VkDevice s_Device;
		static VkSurfaceKHR s_Surface;
		static Window* s_Window;

		static VkQueue s_GraphicsQueue;
		static VkQueue s_PresentQueue;

		static VkCommandPool s_CommandPool;

		static const std::vector<const char*> s_ValidationLayers;
		static const std::vector<const char*> s_DeviceExtensions;
		static std::vector<Extension> s_OptionalExtensions;

#ifdef DIST
		static const bool s_EnableValidationLayers = false;
#else
		static const bool s_EnableValidationLayers = true;
#endif
	};

}