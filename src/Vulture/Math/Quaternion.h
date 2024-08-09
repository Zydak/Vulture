// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/quaternion.hpp"

namespace Vulture
{
	class Quaternion
	{
	public:
		Quaternion() = default;
		Quaternion(const glm::vec3& angles);
		~Quaternion() = default;

		void Init(const glm::vec3& angles);

		void Reset();

		void SetAngles(const glm::vec3& angles);
		void AddAngles(const glm::vec3& angles);
		glm::vec3 GetAngles() const;

		void SetQuaternion(const glm::quat& quat);

		void AddPitch(float angle);
		void AddYaw(float angle);
		void AddRoll(float angle);

		glm::mat4 GetMat4() const;
		inline glm::quat GetGlmQuat() const { return m_Quat; }

		inline glm::vec3 GetFrontVec() const { return m_FrontVec; };
		inline glm::vec3 GetUpVec() const { return m_UpVec; };
		inline glm::vec3 GetRightVec() const { return m_RightVec; };

		bool operator ==(const Quaternion& other) const;
		bool operator !=(const Quaternion& other) const;
		
	private:
		void UpdateVectors();

		glm::quat m_Quat{1.0f, 0.0f, 0.0f, 0.0f};
		glm::vec3 m_FrontVec{0.0f, 0.0f, 1.0f};
		glm::vec3 m_UpVec{0.0f, 1.0f, 0.0f};
		glm::vec3 m_RightVec{1.0f, 0.0f, 0.0f};
	};
}