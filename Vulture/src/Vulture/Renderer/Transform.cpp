#include "pch.h"
#include "Transform.h"
#include "glm/gtc/matrix_transform.hpp"

namespace Vulture
{

	Transform::Transform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
		: m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
	{

	}

	Transform::~Transform()
	{

	}

	glm::mat4 Transform::GetMat4()
	{
		glm::mat4 mat{1.0f};
		mat = glm::translate(mat, m_Translation);
		mat = glm::rotate(mat, glm::radians(m_Rotation.x), glm::vec3(0.0f, 1.0f, 0.0f));
		mat = glm::rotate(mat, glm::radians(m_Rotation.y), glm::vec3(1.0f, 0.0f, 1.0f));
		mat = glm::rotate(mat, glm::radians(m_Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
		mat = glm::scale(mat, m_Scale);

		return mat;
	}

}