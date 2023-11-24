#include "pch.h"
#include "Transform.h"

namespace Vulture
{

	Transform::Transform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
		: m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
	{
		SetTranslation(m_Translation);
		SetRotation(m_Rotation);
		SetScale(m_Scale);
	}

	Transform::~Transform()
	{

	}
}