#pragma once
#include "pch.h"

#include "Vulture/Utility/Utility.h"
#include "Vulkan/Device.h"

namespace Vulture
{
	struct ApplicationInfo
	{
		uint32_t WindowWidth = 600;
		uint32_t WindowHeight = 600;
		uint32_t MaxFramesInFlight = 2;
		std::string WorkingDirectory = "";
		std::string Name = "";
		std::string Icon = "";
		bool EnableRayTracingSupport = false;
		bool UseMemoryAddress = true;
		std::vector<const char*> DeviceExtensions;
		std::vector<const char*> OptionalExtensions;
		VkPhysicalDeviceFeatures2 Features = VkPhysicalDeviceFeatures2();
	};

	class Application
	{
	public:
		Application(const ApplicationInfo& appInfo);
		virtual ~Application();

		virtual void Destroy() = 0;
		virtual void OnUpdate(double delta) = 0;

		void Run();
	protected:

		ApplicationInfo m_ApplicationInfo;
		Ref<Window> m_Window;
	};

	// defined by client
	Application* CreateApplication();

}