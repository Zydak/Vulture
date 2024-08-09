// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "Input.h"

#include "imgui.h"

namespace Vulture
{
	static float scrollValue = 0.0f;

	static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
	{
		scrollValue += (float)yoffset;

		ImGuiIO* io = &ImGui::GetIO();
		io->MouseWheel += (float)yoffset;
	}

	void Input::Init(GLFWwindow* window)
	{
		s_Window = window;

		glfwSetScrollCallback(window, ScrollCallback);
	}

	bool Input::IsKeyPressed(int keyCode)
	{
		return glfwGetKey(s_Window, keyCode);
	}

	bool Input::IsMousePressed(int mouseButton)
	{
		return glfwGetMouseButton(s_Window, mouseButton);
	}

	glm::vec2 Input::GetMousePosition()
	{
		double x, y;
		glfwGetCursorPos(s_Window, &x, &y);
		return glm::vec2(x, y);
	}

	float Input::GetScrollValue()
	{
		return scrollValue;
	}

	void Input::SetScrollValue(float x)
	{
		scrollValue = x;
	}

	GLFWwindow* Input::s_Window;

}