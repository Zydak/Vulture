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

	GLFWwindow* Input::s_Window;
}