#pragma once
#include "pch.h"
#include "glm/glm.hpp"

namespace Vulture
{

	class Transform
	{
	public:
		Transform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
		~Transform();

		glm::mat4 GetMat4();

	private:
		glm::vec3 m_Translation;
		glm::vec3 m_Rotation;
		glm::vec3 m_Scale;
	};

}

