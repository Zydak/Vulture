#include "pch.h"
#include "Utility/Utility.h"

#include "Window.h"

namespace Vulture
{

	void GLFW_CALLBACK(int errCode, const char* message)
	{
		VL_CORE_ERROR("GLFW ERROR CODE: {0}", errCode);
		VL_CORE_ERROR("{0}", message);
	}

	Window::Window(int width, int height, std::string name) : m_Width(width), m_Height(height), m_Name(name)
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		m_Window = glfwCreateWindow(m_Width, m_Height, m_Name.c_str(), nullptr, nullptr);
		glfwSetWindowUserPointer(m_Window, this);
		glfwSetFramebufferSizeCallback(m_Window, ResizeCallback);
		glfwSetErrorCallback(GLFW_CALLBACK);

		GLFWmonitor** monitorwRaw = glfwGetMonitors(&m_MonitorsCount);
		m_Monitors.resize(m_MonitorsCount);
		m_MonitorRawNames.resize(m_MonitorsCount);
		for (int i = 0; i < m_MonitorsCount; i++)
		{
			m_Monitors[i].Monitor = monitorwRaw[i];
			m_Monitors[i].Name = glfwGetMonitorName(m_Monitors[i].Monitor);
			m_MonitorRawNames[i] = glfwGetMonitorName(m_Monitors[i].Monitor);
		}
	}

	Window::~Window()
	{
		glfwDestroyWindow(m_Window);
		glfwTerminate();
	}

	void Window::CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface)
	{
		VL_CORE_RETURN_ASSERT(glfwCreateWindowSurface(instance, m_Window, nullptr, surface),
			VK_SUCCESS, 
			"failed to create window surface"
		);
	}

	void Window::PollEvents()
	{
		glfwPollEvents();
	}

	void Window::Resize(const glm::vec2& extent)
	{
		m_Resized = true;
		m_Width = (int)extent.x;
		m_Height = (int)extent.y;
		glfwSetWindowSize(m_Window, m_Width, m_Height);
	}

	void Window::SetFullscreen(bool val, GLFWmonitor* Monitor)
	{
		if (val)
			glfwSetWindowMonitor(m_Window, Monitor, 0, 0, m_Width, m_Height, GLFW_DONT_CARE);
		else
			glfwSetWindowMonitor(m_Window, NULL, 0, 0, m_Width, m_Height, GLFW_DONT_CARE);
	}

	void Window::ResizeCallback(GLFWwindow* window, int width, int height) {
		auto _window = (Window*)(glfwGetWindowUserPointer(window));
		_window->m_Resized = true;
		_window->m_Width = width;
		_window->m_Height = height;
	}

}