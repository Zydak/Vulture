#include "pch.h"
#include "Input.h"

namespace Vulture
{

	void Input::Init(GLFWwindow* window)
	{
		s_Window = window;
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

	GLFWwindow* Input::s_Window;

}