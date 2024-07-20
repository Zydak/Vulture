#include "pch.h"
#include "Application.h"
#include "Renderer/Renderer.h"
#include "Asset/AssetManager.h"
#include "Input.h"
#include "Vulkan/DeleteQueue.h"

namespace Vulture
{
	Application::Application(const ApplicationInfo& appInfo)
		: m_ApplicationInfo(appInfo)
	{
		float r1 = 0.1245f;
		float r2 = 0.7f;

		float theta = 2.0f * 3.14 * r1;
		float phi = glm::acos(2.0f * r2 - 1.0f) - 0.5f * 3.14;

		float x = glm::cos(phi) * glm::sin(theta);
		float y = glm::cos(phi);
		float z = glm::cos(phi) * glm::cos(theta);

		if (!appInfo.WorkingDirectory.empty())
			std::filesystem::current_path(appInfo.WorkingDirectory);

		Logger::Init();
		VL_CORE_TRACE("LOGGER INITIALIZED");
		Window::CreateInfo winInfo;
		winInfo.Width = (int)appInfo.WindowWidth;
		winInfo.Height = (int)appInfo.WindowHeight;
		winInfo.Name = appInfo.Name;
		winInfo.Icon = appInfo.Icon;
		m_Window = std::make_shared<Window>(winInfo);

		Device::CreateInfo deviceInfo{};
		deviceInfo.DeviceExtensions = appInfo.DeviceExtensions;
		deviceInfo.OptionalExtensions = appInfo.OptionalExtensions;
		deviceInfo.Features = appInfo.Features;
		deviceInfo.Window = m_Window.get();
		deviceInfo.UseRayTracing = appInfo.EnableRayTracingSupport;
		deviceInfo.UseMemoryAddress = appInfo.UseMemoryAddress;
		Device::Init(deviceInfo);
		Renderer::Init(*m_Window, appInfo.MaxFramesInFlight);
		Input::Init(m_Window->GetGLFWwindow());

		const uint32_t coresCount = std::thread::hardware_concurrency();
		AssetManager::Init({ coresCount / 2 });
		DeleteQueue::Init({appInfo.MaxFramesInFlight});
	}

	Application::~Application()
	{
		VL_CORE_INFO("Closing");
	}

	void Application::Run()
	{
		VL_CORE_TRACE("\n\n\n\nMAIN LOOP START\n\n\n\n");
		Timer timer;
		double deltaTime = 0.0f;

		while (!m_Window->ShouldClose())
		{
			timer.Reset();
			m_Window->PollEvents();

			OnUpdate(deltaTime);
			deltaTime = timer.ElapsedSeconds();

			DeleteQueue::UpdateQueue();
		}

		vkDeviceWaitIdle(Device::GetDevice());

		Renderer::Destroy();
		Destroy();
		DeleteQueue::Destroy();
		Device::Destroy();
	}
}