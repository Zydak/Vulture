#include "pch.h"
#include "Input.h"

namespace Vulture
{
	static float scrollValue = 0.0f;

	static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
	{
		scrollValue += yoffset;
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