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

	class Window 
	{
	public:

		struct CreateInfo
		{
			int Width;
			int Height;
			std::string Name;
			std::string Icon;
		};

		void Init(CreateInfo& createInfo);
		void Destroy();

		Window() = default;
		Window(CreateInfo& createInfo);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		void CreateWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
		void PollEvents();

		inline bool WasWindowResized() const { return m_Resized; }
		inline bool ShouldClose() const { return glfwWindowShouldClose(m_Window); }

		inline void ResetWindowResizedFlag() { m_Resized = false; }
		inline void Close() { glfwSetWindowShouldClose(m_Window, GLFW_TRUE); }

		inline VkExtent2D GetExtent() const { return { (uint32_t)m_Width, (uint32_t)m_Height }; }
		inline float GetAspectRatio() const { return { (float)m_Width / (float)m_Height }; }
		inline const std::vector<Monitor>& GetMonitors() const { return m_Monitors; }
		inline const std::vector<const char*>& GetMonitorsNames()  const { return m_MonitorRawNames; }
		inline int GetMonitorsCount() const { return m_MonitorsCount; }

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
		std::vector<const char*> m_MonitorRawNames; // ³ot?
		int m_MonitorsCount;

		bool m_Initialized = false;
	};

}