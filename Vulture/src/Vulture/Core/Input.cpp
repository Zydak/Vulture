#include "pch.h"
#include "Input.h"

namespace Vulture
{

	void Input::Init(GLFWwindow* window)
	{
		s_Window = window;
		m_KeysPressed.resize(349);
	}

	bool Input::IsKeyPressed(int keyCode)
	{
		if (m_KeysPressed[keyCode] != true)
		{
			m_KeysPressed[keyCode] = true;
			return glfwGetKey(s_Window, keyCode);
		}
		else
		{
			return false;
		}
	}

	void Input::ResetInput()
	{
		for (int i = 0; i < m_KeysPressed.size(); i++)
		{
			m_KeysPressed[i] = false;
		}
	}

	GLFWwindow* Input::s_Window;
	std::vector<bool> Input::m_KeysPressed;

}