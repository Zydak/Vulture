#include "pch.h"
#include "Application.h"
#include "Renderer/Renderer.h"


namespace Vulture
{

	Application::Application(const ApplicationInfo& appInfo, float width, float height)
		: m_ApplicationInfo(appInfo)
	{
		if (!appInfo.WorkingDirectory.empty())
			std::filesystem::current_path(appInfo.WorkingDirectory);

		Logger::Init();
		VL_CORE_TRACE("LOGGER INITIALIZED");
		m_Window = std::make_shared<Window>((int)width, (int)height, appInfo.Name);
		Device::Init(*m_Window);
		Renderer::Init(*m_Window);
	}

	Application::~Application()
	{

	}

	void Application::Run()
	{
		while (!m_Window->ShouldClose())
		{
			m_Window->PollEvents();

			OnUpdate(0.0f);

			Renderer::Render();
		}

		Renderer::Destroy();
	}
}