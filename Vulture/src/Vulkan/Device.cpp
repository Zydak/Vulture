#include "pch.h"
#include "Utility/Utility.h"

#define VMA_IMPLEMENTATION
#include "Device.h"

#include "GLFW/glfw3.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{

	const std::vector<const char*> Device::s_ValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> Device::s_DeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	std::vector<Extension> Device::s_OptionalExtensions = { 
		{VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME , false},
		{VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME, false}
	};

	/*
	   *  @brief Callback function for Vulkan to be called by validation layers when needed
	*/
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		{
			VL_CORE_INFO("Validation Layer: Info\n\t{0}", pCallbackData->pMessage);
		}
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			VL_CORE_ERROR("Validation Layer: Validation Error\n\t{0}", pCallbackData->pMessage);
			//VL_CORE_ASSERT(false, ""); // Vulkan error
		}
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		{
			VL_CORE_WARN("Validation Layer: Warning\n\t{0}", pCallbackData->pMessage);
		}
		return VK_FALSE;
	}

	/*
	 * @brief Loads vkCreateDebugUtilsMessengerEXT from memory and then calls it.
	 * This is necessary because this function is an extension function, it is not automatically loaded to memory
	 */
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) { return func(instance, pCreateInfo, pAllocator, pDebugMessenger); }
		else { return VK_ERROR_EXTENSION_NOT_PRESENT; }
	}

	/*
	 * @brief Loads vkDestroyDebugUtilsMessengerEXT from memory and then calls it.
	 * This is necessary because this function is an extension function, it is not automatically loaded to memory
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
		CreateMemoryAllocator();
	}

	Device::~Device()
	{
		vkDestroyCommandPool(s_Device, s_CommandPool, nullptr);
		vkDestroyDevice(s_Device, nullptr);
		if (s_EnableValidationLayers) { DestroyDebugUtilsMessengerEXT(s_Instance, s_DebugMessenger, nullptr); }

		vkDestroySurfaceKHR(s_Instance, s_Surface, nullptr);
		vkDestroyInstance(s_Instance, nullptr);
	}

	/*
	 * @brief Checks if the required Vulkan validation layers are supported.
	 *
	 * @return True if all required validation layers are supported, false otherwise.
	 */
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

	/*
	 * @brief Retrieves the required Vulkan instance extensions for GLFW integration.
	 *
	 * @return A vector of const char* containing the required Vulkan extensions.
	 */
	std::vector<const char*> Device::GetRequiredGlfwExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (s_EnableValidationLayers) { extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); }

		return extensions;
	}

	/*
	 * @brief Queries the available Vulkan instance extension properties and verifies that
	 * all the required extensions for GLFW integration are present.
	 */
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
			if (available.find(required) == available.end()) 
			{ 
				VL_CORE_ASSERT(false, "Missing glfw extension: {0}", required);
			}
		}
	}

	/*
	 * @brief Sets up the Vulkan Debug Utils Messenger for validation layers.
	 *
	 * @note If validation layers are not enabled, the function returns without setting up the debug messenger.
	 */
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

	/*
	 * @brief Sets which messages to show by validation layers
	 *
	 * @param createInfo struct to fill
	*/
	void Device::PopulateDebugMessenger(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
	}

	/*
	 * @brief Creates the Vulkan instance for the application.
	 *
	 * This function creates the Vulkan instance for the application, configuring application and engine information,
	 * enabling validation layers if requested, and checking for required GLFW extensions. It uses Vulkan API calls
	 * to create the instance and performs necessary checks for successful creation.
	 */
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

	/*
	 * @brief Finds the Vulkan queue families supported by the physical device.
	 *
	 * @param device - The Vulkan physical device for which queue families are being queried.
	 *
	 * @return QueueFamilyIndices structure containing the indices of the graphics and present queue families.
	 */
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

	void Device::CreateMemoryAllocator()
	{
		VmaAllocatorCreateInfo allocatorInfo{};
		allocatorInfo.instance = s_Instance;
		allocatorInfo.physicalDevice = s_PhysicalDevice;
		allocatorInfo.device = s_Device;
		if (s_OptionalExtensions[1].supported) // element 1 has to be VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME
			allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;

		vmaCreateAllocator(&allocatorInfo, &s_Allocator);
	}

	void Device::CreateMemoryPool(uint32_t memoryIndex, VmaPool& pool, uint32_t MBSize)
	{
		VmaPoolCreateInfo poolInfo{};
		poolInfo.blockSize = 1024 * 1024 * MBSize;
		poolInfo.memoryTypeIndex = memoryIndex;
		poolInfo.priority = 0.5f;
		vmaCreatePool(s_Allocator, &poolInfo, &pool);
	}

	void Device::FindMemoryTypeIndex(VkMemoryPropertyFlags flags, uint32_t& memoryIndex)
	{
		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.priority = 0.5f;

		vmaFindMemoryTypeIndex(s_Allocator, flags, &allocInfo, &memoryIndex);
	}

	void Device::FindMemoryTypeIndexForBuffer(VkBufferCreateInfo& createInfo, uint32_t& memoryIndex, VkMemoryPropertyFlags flags)
	{
		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.priority = 0.5f;
		if (flags)
			allocInfo.requiredFlags = flags;

		vmaFindMemoryTypeIndexForBufferInfo(s_Allocator, &createInfo, &allocInfo, &memoryIndex);
	}

	void Device::FindMemoryTypeIndexForImage(VkImageCreateInfo& createInfo, uint32_t& memoryIndex, VkMemoryPropertyFlags flags)
	{
		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.priority = 0.5f;
		if (flags)
			allocInfo.requiredFlags = flags;

		vmaFindMemoryTypeIndexForImageInfo(s_Allocator, &createInfo, &allocInfo, &memoryIndex);
	}

	void Device::CreateBuffer(VkBufferCreateInfo& createInfo, VkBuffer& buffer, VmaAllocation& alloc, VkMemoryPropertyFlags customFlags)
	{
		uint32_t memoryIndex = 0;
		FindMemoryTypeIndexForBuffer(createInfo, memoryIndex, customFlags);
		auto it = s_Pools.find(memoryIndex);
		if (it != s_Pools.end())
		{
			// pool found
			VmaAllocationCreateInfo allocCreateInfo = {};
			allocCreateInfo.pool = it->second;
			allocCreateInfo.priority = 0.5f;

			vmaCreateBuffer(s_Allocator, &createInfo, &allocCreateInfo, &buffer, &alloc, nullptr);
		}
		else
		{
			s_Pools[memoryIndex] = VmaPool();
			CreateMemoryPool(memoryIndex, s_Pools[memoryIndex]);

			VmaAllocationCreateInfo allocCreateInfo = {};
			allocCreateInfo.pool = s_Pools[memoryIndex];
			allocCreateInfo.priority = 0.5f;
			vmaCreateBuffer(s_Allocator, &createInfo, &allocCreateInfo, &buffer, &alloc, nullptr);
		}
	}

	void Device::CreateImage(VkImageCreateInfo& createInfo, VkImage& image, VmaAllocation& alloc, VkMemoryPropertyFlags customFlags)
	{
		uint32_t memoryIndex = 0;
		FindMemoryTypeIndexForImage(createInfo, memoryIndex, customFlags);
		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.memoryTypeBits = memoryIndex;
		VL_CORE_RETURN_ASSERT(
			vmaCreateImage(s_Allocator, &createInfo, &allocCreateInfo, &image, &alloc, nullptr),
			VK_SUCCESS,
			"Couldn't create an image!"
		);
	}

	/*
	 * @brief Evaluates the suitability of the specified Vulkan physical device based on criteria
	 * such as queue families, required extensions, and swap chain support. It returns true if the device
	 * is deemed suitable, and false otherwise.
	 *
	 * @param device - The Vulkan physical device to be evaluated for suitability.
	 *
	 * @return True if the device is suitable, false otherwise.
	 */
	bool Device::IsDeviceSuitable(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices = FindQueueFamilies(device);

		bool extensionsSupported = CheckDeviceExtensionSupport(device);

		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapchainSupportDetails swapChainSupport = QuerySwapchainSupport(device);
			swapChainAdequate = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
		}

		return indices.IsComplete() && extensionsSupported && swapChainAdequate;
	}

	/*
	 * @brief Enumerates the available Vulkan physical devices, assesses their suitability using
	 * the IsDeviceSuitable function, and selects the most appropriate device based on certain criteria.
	 * It sets the selected physical device handle in the s_PhysicalDevice member variable.
	 *
	 * @note The function prioritizes discrete GPUs over integrated GPUs when both are available.
	 */
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

	/*
	 * @brief Creates the logical Vulkan device, including the necessary queues, based on the selected
	 * physical device and provided configuration settings.
	 *
	 * @note The function checks for the required queue families using the FindQueueFamilies function.
	 * @note The function handles the selection of supported device extensions and optional extensions.
	 */
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

		vkGetPhysicalDeviceFeatures(s_PhysicalDevice, &s_Features);

		VkPhysicalDeviceFeatures deviceFeatures = {};

		std::vector<const char*> extensions;
		for (auto& extension : s_DeviceExtensions)
		{
			extensions.push_back(extension);
		}

		for (auto& extension : s_OptionalExtensions)
		{
			if (extension.supported)
				extensions.push_back(extension.Name);
		}

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = (uint32_t)extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

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

	/*
	 * @brief Creates the Vulkan surface for rendering.
	 */
	void Device::CreateSurface() 
	{ 
		s_Window->CreateWindowSurface(s_Instance, &s_Surface); 
	}

	/*
	 * @brief Queries the physical device for its supported device extensions and verifies
	 * that all the required extensions are present. It updates the support status of optional extensions
	 * accordingly. The function returns true if all required extensions are supported, and false otherwise.
	 *
	 * @param device - The Vulkan physical device for which extension support is being checked.
	 *
	 * @return True if all required extensions are supported, false otherwise.
	 */
	bool Device::CheckDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(s_DeviceExtensions.begin(), s_DeviceExtensions.end());

		for (const auto& extension : availableExtensions) { requiredExtensions.erase(extension.extensionName); }
		for (const auto& extension : availableExtensions) 
		{
			Extension ext;
			ext.Name = extension.extensionName;
			for (auto& optionalEtension : s_OptionalExtensions)
			{
				if (std::string(optionalEtension.Name) == std::string(ext.Name))
				{
					optionalEtension.supported = true;
				}
			}
		}

		return requiredExtensions.empty();
	}

	/*
	 * @brief Queries the Vulkan physical device for its swap chain support details, including
	 * capabilities, supported surface formats, and present modes. It returns a SwapchainSupportDetails
	 * structure containing the obtained information.
	 *
	 * @param device - The Vulkan physical device for which swap chain support is being queried.
	 *
	 * @return SwapchainSupportDetails structure containing the swap chain support details.
	 */
	SwapchainSupportDetails Device::QuerySwapchainSupport(VkPhysicalDevice device)
	{
		SwapchainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, s_Surface, &details.Capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Surface, &formatCount, nullptr);
		if (formatCount != 0) 
		{
			details.Formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, s_Surface, &formatCount, details.Formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Surface, &presentModeCount, nullptr);
		if (presentModeCount != 0) 
		{
			details.PresentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, s_Surface, &presentModeCount, details.PresentModes.data());
		}

		return details;
	}

	/*
	 * @brief Iterates through the provided list of Vulkan formats and checks if they have the
	 * required properties specified by the tiling and features parameters. It returns the first supported
	 * format found, or VK_FORMAT_UNDEFINED if none are supported.
	 *
	 * @param candidates - The list of Vulkan formats to check for support.
	 * @param tiling - The desired image tiling mode (VK_IMAGE_TILING_LINEAR or VK_IMAGE_TILING_OPTIMAL).
	 * @param features - The desired format features.
	 *
	 * @return Supported Vulkan format from the candidates with the specified properties.
	 */
	VkFormat Device::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(s_PhysicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) { return format; }
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) { return format; }
		}
		return VK_FORMAT_UNDEFINED;
	}

	/*
	 * @brief Creates a Vulkan command pool based on the graphics queue family index.
	 * It sets the command pool creation flags to allow resetting command buffers.
	 *
	 * @note The function asserts if the command pool creation fails.
	 */
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

	/*
	 * @brief Iterates through the available memory types on the physical device and checks
	 * for a suitable type that matches the specified type filter and properties. It returns the index
	 * of the found memory type.
	 *
	 * @param typeFilter - Bit field specifying the memory types that are suitable.
	 * @param properties - Desired memory properties.
	 *
	 * @return Index of the suitable memory type.
	 *
	 * @note It asserts if no suitable memory type is found.
	 */
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

	/*
	 * @brief Function allocates a command buffer from the specified command pool and begins recording
	 * commands in it. The command buffer is set to the VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	 * flag, indicating that it will be used for a single submission.
	 *
	 * @param buffer - Vulkan command buffer to be allocated and recorded.
	 * @param pool - Vulkan command pool from which the command buffer is allocated.
	 */
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

	/**
	 * @brief Ends the recording of the specified command buffer and submits it to the specified
	 * queue for immediate execution. It waits for the queue to become idle before freeing the command buffer.
	 *
	 * @param commandBuffer - Vulkan command buffer to be ended, submitted, and freed.
	 * @param queue - Vulkan queue to which the command buffer is submitted.
	 * @param pool - Vulkan command pool from which the command buffer was allocated.
	 */
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

	VmaAllocator Device::s_Allocator;
	std::unordered_map<uint32_t, VmaPool> Device::s_Pools;
	VkPhysicalDeviceProperties Device::s_Properties = {};
	VkPhysicalDeviceFeatures Device::s_Features;
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