#include "pch.h"
#include "Application.h"
#include "Renderer/Renderer.h"
#include "Renderer/AssetManager.h"
#include "Input.h"

namespace Vulture
{
	Application::Application(const ApplicationInfo& appInfo, float width, float height)
		: m_ApplicationInfo(appInfo)
	{
		if (!appInfo.WorkingDirectory.empty())
			std::filesystem::current_path(appInfo.WorkingDirectory);

		Logger::Init();
		VL_CORE_TRACE("LOGGER INITIALIZED");
		Window::CreateInfo winInfo;
		winInfo.Width = (int)width;
		winInfo.Height = (int)height;
		winInfo.Name = appInfo.Name;
		winInfo.Icon = appInfo.Icon;
		m_Window = std::make_shared<Window>(winInfo);
		Device::Init(*m_Window, appInfo.RayTracingSupport);
		Renderer::Init(*m_Window);
		Input::Init(m_Window->GetGLFWwindow());
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
			AssetManager::Cleanup();
			deltaTime = timer.ElapsedSeconds();
		}

		vkDeviceWaitIdle(Device::GetDevice());

		Renderer::Destroy();
		Destroy();
		AssetManager::Destroy();
		Device::Destroy();
	}
}