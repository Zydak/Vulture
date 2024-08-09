// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "Quaternion.h"

namespace Vulture
{
	Quaternion::Quaternion(const glm::vec3& angles)
	{
		Init(angles);
	}

	void Quaternion::Init(const glm::vec3& angles)
	{
		m_Quat = glm::quat(glm::radians(angles));
	}

	void Quaternion::Reset()
	{
		m_Quat = { 1.0f, 0.0f, 0.0f, 0.0f };
		m_FrontVec = { 0.0f, 0.0f, 1.0f };
		m_UpVec = { 0.0f, 1.0f, 0.0f };
		m_RightVec = { 1.0f, 0.0f, 0.0f };
	}

	void Quaternion::SetAngles(const glm::vec3& angles)
	{
		m_Quat = glm::quat(glm::radians(angles));
		m_Quat = glm::normalize(m_Quat);

		UpdateVectors();
	}

	void Quaternion::AddAngles(const glm::vec3& angles)
	{
		AddPitch(angles.x);
		AddYaw(angles.y);
		AddRoll(angles.z);

		UpdateVectors();
	}

	glm::vec3 Quaternion::GetAngles() const
	{
		return glm::degrees(glm::eulerAngles(m_Quat));
	}

	void Quaternion::SetQuaternion(const glm::quat& quat)
	{
		m_Quat = quat;

		UpdateVectors();
	}

	void Quaternion::AddPitch(float angle)
	{
		glm::quat quat;
		quat.w =  glm::cos(glm::radians(angle));
		quat.x = (glm::sin(glm::radians(angle)) * m_RightVec.x);
		quat.y = (glm::sin(glm::radians(angle)) * m_RightVec.y);
		quat.z = (glm::sin(glm::radians(angle)) * m_RightVec.z);

		m_Quat = glm::normalize(m_Quat);
		m_Quat = m_Quat * quat;

		UpdateVectors();
	}

	void Quaternion::AddYaw(float angle)
	{
		glm::quat quat;
		quat.w =  glm::cos(glm::radians(angle));
		quat.x = (glm::sin(glm::radians(angle)) * m_UpVec.x);
		quat.y = (glm::sin(glm::radians(angle)) * m_UpVec.y);
		quat.z = (glm::sin(glm::radians(angle)) * m_UpVec.z);

		m_Quat = glm::normalize(m_Quat);
		m_Quat = m_Quat * quat;

		UpdateVectors();
	}

	void Quaternion::AddRoll(float angle)
	{
		glm::quat quat;
		quat.w =  glm::cos(glm::radians(angle));
		quat.x = (glm::sin(glm::radians(angle)) * m_FrontVec.x);
		quat.y = (glm::sin(glm::radians(angle)) * m_FrontVec.y);
		quat.z = (glm::sin(glm::radians(angle)) * m_FrontVec.z);

		m_Quat = glm::normalize(m_Quat);
		m_Quat = m_Quat * quat;

		UpdateVectors();
	}

	glm::mat4 Quaternion::GetMat4() const
	{
		return glm::toMat4(m_Quat);
	}

	void Quaternion::UpdateVectors()
	{
		glm::mat4 rotationMat = glm::toMat4(m_Quat);

		m_FrontVec = glm::vec3(rotationMat[0][2], rotationMat[1][2], rotationMat[2][2]);
		m_RightVec = glm::vec3(rotationMat[0][0], rotationMat[1][0], rotationMat[2][0]);
		m_UpVec =	 glm::vec3(rotationMat[0][1], rotationMat[1][1], rotationMat[2][1]);
	}

	bool Quaternion::operator!=(const Quaternion& other) const
	{
		return m_Quat != other.m_Quat;
	}

	bool Quaternion::operator==(const Quaternion& other) const
	{
		return m_Quat == other.m_Quat;
	}

}