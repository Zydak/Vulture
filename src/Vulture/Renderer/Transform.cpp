#include "pch.h"
#include "Transform.h"

namespace Vulture
{

	Transform::Transform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
		: m_Translation(translation), m_Rotation(rotation), m_Scale(scale)
	{
		//SetTranslation(m_Translation); ?????
		//SetRotation(m_Rotation);
		//SetScale(m_Scale);
	}

	Transform::~Transform()
	{

	}

	VkTransformMatrixKHR Transform::GetKhrMat()
	{
		glm::mat4 temp = glm::transpose(GetMat4());
		VkTransformMatrixKHR out_matrix;
		memcpy(&out_matrix, &temp, sizeof(VkTransformMatrixKHR));
		return out_matrix;
	}

	glm::mat4 Transform::GetMat4(const Transform& transform, bool& changed)
	{
		changed = false;
		if (m_Translation != transform.GetTranslation() || m_Rotation != transform.GetRotation() || m_Scale != transform.GetScale())
		{
			glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), m_Translation);
			glm::mat4 rotationMat = m_Rotation.GetMat4();
			glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), m_Scale);

			ModelMatrix = translationMat * rotationMat * scaleMat;

			changed = true;
		}
		return ModelMatrix;
	}

	glm::mat4 Transform::GetMat4(const glm::mat4& compareMat)
	{
		if (ModelMatrix != compareMat)
		{
			glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), m_Translation);
			glm::mat4 rotationMat = m_Rotation.GetMat4();
			glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), m_Scale);

			ModelMatrix = translationMat * rotationMat * scaleMat;
		}

		return ModelMatrix;
	}

	glm::mat4 Transform::GetMat4()
	{
		glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), m_Translation);
		glm::mat4 rotationMat = m_Rotation.GetMat4();
		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), m_Scale);

		ModelMatrix = translationMat * rotationMat * scaleMat;

		return ModelMatrix;
	}

	void Transform::SetTranslation(const glm::vec3& vec)
	{
		m_Translation = vec;
	}

	void Transform::SetRotation(const glm::vec3& vec)
	{
		m_Rotation.SetAngles(vec);
	}

	void Transform::SetScale(const glm::vec3& vec)
	{
		m_Scale = vec;
	}

	void Transform::AddTranslation(const glm::vec3& vec)
	{
		m_Translation += vec;
	}

	void Transform::AddRotation(const glm::vec3& vec)
	{
		m_Rotation.AddAngles(vec);
	}

	void Transform::AddScale(const glm::vec3& vec)
	{
		m_Scale += vec;
	}

}