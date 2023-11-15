#include "pch.h"
#include "Application.h"


namespace Vulture
{

	Application::Application(const ApplicationInfo& appInfo, float width, float height)
		: m_ApplicationInfo(appInfo)
	{
		if (!appInfo.WorkingDirectory.empty())
			std::filesystem::current_path(appInfo.WorkingDirectory);

		Vulture::Logger::Init();
		VL_CORE_INFO("LOGGER INITIALIZED");
		m_Window = std::make_shared<Window>(width, height, appInfo.Name);
		Vulture::Device::Init(*m_Window);
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
		}
	}
}