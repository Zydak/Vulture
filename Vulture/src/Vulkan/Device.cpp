#include "pch.h"
#include "Utility/Utility.h"

#include "Device.h"

#include "GLFW/glfw3.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{

	const std::vector<const char*> Device::s_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> Device::s_DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	/*
	   *  @brief Callback function for Vulkan to be called by validation layers when needed
	*/
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		if (messageType == 0x00000001)
			VL_CORE_INFO("Validation Layer: Info\n\t{0}", pCallbackData->pMessage);
		if (messageType == 0x00000002)
			VL_CORE_ERROR("Validation Layer: Validation Error\n\t{0}", pCallbackData->pMessage);
		if (messageType == 0x00000004)
			VL_CORE_WARN("Validation Layer: Performance Issue (Not Optimal)\n\t{0}", pCallbackData->pMessage);
		return VK_FALSE;
	}

	/**
	   *  @brief This is a proxy function.
	   *  It loads vkCreateDebugUtilsMessengerEXT from memory and then calls it.
	   *  This is necessary because this function is an extension function, it is not automatically loaded to memory
	*/
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) { return func(instance, pCreateInfo, pAllocator, pDebugMessenger); }
		else { return VK_ERROR_EXTENSION_NOT_PRESENT; }
	}

	/**
	   *  @brief This is a proxy function.
	   *  It loads vkDestroyDebugUtilsMessengerEXT from memory and then calls it.
	   *  This is necessary because this function is an extension function, it is not automatically loaded to memory
	*/
	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) { func(instance, debugMessenger, pAllocator); }
	}

	void Device::Init(Window& window)
	{
		Device::s_Window = &window;

		CreateInstance();
		SetupDebugMessenger();
		CreateSurface();
		PickPhysicalDevice();
		CreateLogicalDevice();
		CreateCommandPool();
	}

	Device::~Device()
	{
		vkDestroyCommandPool(s_Device, s_CommandPool, nullptr);
		vkDestroyDevice(s_Device, nullptr);
		if (s_EnableValidationLayers) { DestroyDebugUtilsMessengerEXT(s_Instance, s_DebugMessenger, nullptr); }

		vkDestroySurfaceKHR(s_Instance, s_Surface, nullptr);
		vkDestroyInstance(s_Instance, nullptr);
	}

	bool Device::CheckValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : s_ValidationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}
			if (!layerFound) { return false; }
		}

		return true;
	}

	std::vector<const char*> Device::GetRequiredGlfwExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (s_EnableValidationLayers) { extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); }

		return extensions;
	}

	void Device::CheckRequiredGlfwExtensions()
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::unordered_set<std::string> available;
		for (const auto& extension : extensions) {
			available.insert(extension.extensionName);
		}

		auto requiredExtensions = GetRequiredGlfwExtensions();
		for (const auto& required : requiredExtensions) {
			if (available.find(required) == available.end()) { throw std::runtime_error("Missing required GLFW extension"); }
		}
	}

	void Device::SetupDebugMessenger()
	{
		if (!s_EnableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		PopulateDebugMessenger(createInfo);

		VL_CORE_RETURN_ASSERT(CreateDebugUtilsMessengerEXT(s_Instance, &createInfo, nullptr, &s_DebugMessenger),
			VK_SUCCESS,
			"Failed to setup debug messenger!"
		);
	}

	/**
	   *  @brief Sets which messages to show by validation layers
	   *
	   *  @param createInfo struct to fill
	*/
	void Device::PopulateDebugMessenger(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
	}

	void Device::CreateInstance()
	{
		VL_CORE_ASSERT((s_EnableValidationLayers && CheckValidationLayerSupport()), "Validation layers requested but not available!");

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "SpaceSim";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (s_EnableValidationLayers)
		{
			createInfo.enabledLayerCount = (uint32_t)s_ValidationLayers.size();
			createInfo.ppEnabledLayerNames = s_ValidationLayers.data();

			PopulateDebugMessenger(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateFlagsEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		auto glfwExtensions = GetRequiredGlfwExtensions();
		createInfo.enabledExtensionCount = (uint32_t)glfwExtensions.size();
		createInfo.ppEnabledExtensionNames = glfwExtensions.data();

		VL_CORE_RETURN_ASSERT(vkCreateInstance(&createInfo, nullptr, &s_Instance),
			VK_SUCCESS,
			"failed to create instance!"
		);

		CheckRequiredGlfwExtensions();
	}

	QueueFamilyIndices Device::FindQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
			{
				indices.GraphicsFamily = i;
				indices.GraphicsFamilyHasValue = true;
			}
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, s_Surface, &presentSupport);
			if (presentSupport)
			{
				indices.PresentFamily = i;
				indices.PresentFamilyHasValue = true;
			}
			if (indices.IsComplete())
			{
				break;
			}

			i++;
		}

		return indices;
	}

	bool Device::IsDeviceSuitable(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices = FindQueueFamilies(device);

		bool extensionSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionSupported) {
			SwapchainSupportDetails swapChainSupport = QuerySwapchainSupport(device);
			swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
		}

		return indices.IsComplete() && extensionSupported && swapChainAdequate;
	}

	void Device::PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(s_Instance, &deviceCount, nullptr);
		VL_CORE_ASSERT(deviceCount != 0, "failed to find GPUs with Vulkan support!");
		VL_CORE_INFO("Number of devices: {0}", deviceCount);
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(s_Instance, &deviceCount, devices.data());

		bool discreteFound = false;
		for (const auto& device : devices) {
			if (IsDeviceSuitable(device)) {
				VkPhysicalDeviceProperties properties;
				vkGetPhysicalDeviceProperties(device, &properties);
				if (properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					s_PhysicalDevice = device;
					discreteFound = true;
				}
				else if (properties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && !discreteFound)
				{
					s_PhysicalDevice = device;
				}
			}
		}

		VL_CORE_ASSERT(s_PhysicalDevice != VK_NULL_HANDLE, "failed to find a suitable GPU!");

		vkGetPhysicalDeviceProperties(s_PhysicalDevice, &s_Properties);
		VL_CORE_INFO("physical device: {0}", s_Properties.deviceName);
	}

	void Device::CreateLogicalDevice()
	{
		QueueFamilyIndices indices = FindQueueFamilies(s_PhysicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { indices.GraphicsFamily, indices.PresentFamily };

		std::vector<float> queuePriorities = { 1.0f, 1.0f };
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = queuePriorities.data();
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};
		deviceFeatures.samplerAnisotropy = VK_TRUE;
		deviceFeatures.depthClamp = VK_TRUE;
		deviceFeatures.imageCubeArray = VK_TRUE;

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = (uint32_t)s_DeviceExtensions.size();
		createInfo.ppEnabledExtensionNames = s_DeviceExtensions.data();

		if (s_EnableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(s_ValidationLayers.size());
			createInfo.ppEnabledLayerNames = s_ValidationLayers.data();
		}
		else { createInfo.enabledLayerCount = 0; }

		VL_CORE_RETURN_ASSERT(vkCreateDevice(s_PhysicalDevice, &createInfo, nullptr, &s_Device),
			VK_SUCCESS, 
			"failed to create logical device!"
		);

		vkGetDeviceQueue(s_Device, indices.GraphicsFamily, 0, &s_GraphicsQueue);
		vkGetDeviceQueue(s_Device, indices.PresentFamily, 0, &s_PresentQueue);
	}

	void Device::CreateSurface() { s_Window->CreateWindowSurface(s_Instance, &s_Surface); }

	bool Device::CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(s_DeviceExtensions.begin(), s_DeviceExtensions.end());

		for (const auto& extension : availableExtensions) { requiredExtensions.erase(extension.extensionName); }

		return requiredExtensions.empty();
	}

	SwapchainSupportDetails Device::QuerySwapchainSupport(VkPhysicalDevice device)
	{
		SwapchainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, s_Surface, &details.Capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Surface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Surface, &formatCount, details.Formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Surface, &presentModeCount, nullptr);
		if (presentModeCount != 0) {
			details.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Surface, &presentModeCount, details.PresentModes.data());
		}

		return details;
	}

	VkFormat Device::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(s_PhysicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) { return format; }
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) { return format; }
		}
		VL_CORE_ASSERT(false, "failed to find supported format!");
		return VK_FORMAT_UNDEFINED;
	}

	void Device::CreateCommandPool()
	{
		QueueFamilyIndices queueFamilyIndices = FindPhysicalQueueFamilies();

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.GraphicsFamily;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		VL_CORE_RETURN_ASSERT(vkCreateCommandPool(s_Device, &poolInfo, nullptr, &s_CommandPool),
			VK_SUCCESS,
			"failed to create command pool!"
		);
	}

	uint32_t Device::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		/*
			The VkPhysicalDeviceMemoryProperties structure has two arrays memoryTypes and memoryHeaps.
			Memory heaps are distinct memory resources like dedicated VRAM and swap space in RAM
			for when VRAM runs out. The different types of memory exist within these heaps.
			Right now we'll only concern ourselves with the type of memory and not the heap it comes
			from, but you can imagine that this can affect performance.
		*/
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(s_PhysicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			/*
				The typeFilter parameter will be used to specify the bit field of memory
				types that are suitable. That means that we can find the index of a suitable
				memory type by simply iterating over them and checking if the corresponding bit is set to 1.
			*/
			/*
				We also need to be able to write our vertex data to that memory.
				The memoryTypes array consists of VkMemoryType structs that specify
				the heap and properties of each type of memory. The properties define
				special features of the memory, like being able to map it so we can write to it from the CPU.
				So we should check if the result of the bitwise AND is equal to the desired properties bit field.
			*/
			if ((typeFilter & (1 << i)) && ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)) { return i; }
		}

		VL_CORE_ASSERT(false, "failed to find suitable memory type!");
		return 0;
	}

	void Device::BeginSingleTimeCommands(VkCommandBuffer& buffer, VkCommandPool pool)
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = pool;
		allocInfo.commandBufferCount = 1;

		vkAllocateCommandBuffers(s_Device, &allocInfo, &buffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(buffer, &beginInfo);
	}

	void Device::EndSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue);

		vkFreeCommandBuffers(s_Device, pool, 1, &commandBuffer);
	}

	VkPhysicalDeviceProperties Device::s_Properties = {};
	VkInstance Device::s_Instance = {};
	VkDebugUtilsMessengerEXT Device::s_DebugMessenger = {};
	VkPhysicalDevice Device::s_PhysicalDevice = {};
	VkDevice Device::s_Device = {};
	VkSurfaceKHR Device::s_Surface = {};
	Window* Device::s_Window = {};
	VkQueue Device::s_GraphicsQueue = {};
	VkQueue Device::s_PresentQueue = {};
	VkCommandPool Device::s_CommandPool = {};

}