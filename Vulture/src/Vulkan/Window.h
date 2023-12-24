#pragma once
#include "pch.h"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

namespace Vulture
{

	struct Monitor
	{
		std::string Name;
		GLFWmonitor* Monitor;
	};

	struct WindowInfo
	{
		int Width;
		int Height;
		std::string Name;
		std::string Icon;
	};

	class Window {
	public:
		Window(WindowInfo& winInfo);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
		void PollEvents();

		inline bool WasWindowResized() { return m_Resized; }
		inline bool ShouldClose() { return glfwWindowShouldClose(m_Window); }

		inline void ResetWindowResizedFlag() { m_Resized = false; }
		inline void Close() { glfwSetWindowShouldClose(m_Window, GLFW_TRUE); }

		inline VkExtent2D GetExtent() { return { (uint32_t)m_Width, (uint32_t)m_Height }; }
		inline float GetAspectRatio() const { return { (float)m_Width / (float)m_Height }; }
		inline std::vector<Monitor>& GetMonitors() { return m_Monitors; }
		inline std::vector<const char*>& GetMonitorsNames() { return m_MonitorRawNames; }
		inline int GetMonitorsCount() { return m_MonitorsCount; }

		void Resize(const glm::vec2& extent);
		void SetFullscreen(bool val, GLFWmonitor* monitor);

		inline GLFWwindow* GetGLFWwindow() const { return m_Window; }

	private:
		static void ResizeCallback(GLFWwindow* window, int width, int height);

		int m_Width;
		int m_Height;
		std::string m_Name;
		bool m_Resized = false;

		GLFWwindow* m_Window;
		std::vector<Monitor> m_Monitors;
		std::vector<const char*> m_MonitorRawNames;
		int m_MonitorsCount;
	};

}