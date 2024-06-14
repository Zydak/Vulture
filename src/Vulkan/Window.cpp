#include "pch.h"
#include "Utility/Utility.h"

#include "Window.h"
#include "stbimage/stb_image.h"

namespace Vulture
{

	void GLFW_CALLBACK(int errCode, const char* message)
	{
		VL_CORE_ERROR("GLFW ERROR CODE: {0}", errCode);
		VL_CORE_ERROR("{0}", message);
	}

	void Window::Init(CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		m_Width = createInfo.Width;
		m_Height = createInfo.Height;
		m_Name = createInfo.Name;

		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		m_Window = glfwCreateWindow(m_Width, m_Height, m_Name.c_str(), nullptr, nullptr);

		int left, top, right, bottom;
		glfwGetWindowFrameSize(m_Window, &left, &top, &right, &bottom);

		m_Height -= top + bottom;
		m_Width -= left + right;

		glfwSetWindowSize(m_Window, m_Width, m_Height);

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

		int width, height, channels;
		if (createInfo.Icon == "")
		{
			createInfo.Icon = "../Vulture/assets/Icon.png";
		}
		unsigned char* iconData = stbi_load(createInfo.Icon.c_str(), &width, &height, &channels, STBI_rgb_alpha);
		if (iconData)
		{
			GLFWimage icon;
			icon.width = width;
			icon.height = height;
			icon.pixels = iconData;

			glfwSetWindowIcon(m_Window, 1, &icon);

			stbi_image_free(iconData);
		}

		m_Initialized = true;
	}

	void Window::Destroy()
	{
		glfwDestroyWindow(m_Window);
		glfwTerminate();

		m_Initialized = false;
	}

	/**
	 * @brief Constructs a Window object with the specified window information.
	 * 
	 * @param winInfo - The window information, including width, height, name, and optional icon.
	 */
	Window::Window(CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	Window::~Window()
	{
		if (m_Initialized)
			Destroy();
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

		int left, top, right, bottom;
		glfwGetWindowFrameSize(m_Window, &left, &top, &right, &bottom);

		m_Height -= top + bottom;
		m_Width -= left + right;

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