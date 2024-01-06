#pragma once
#include "pch.h"

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/gtc/matrix_transform.hpp"

#include "Math/Quaternion.h"

namespace Vulture
{
	class Transform
	{
	public:
		Transform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
		~Transform();

		VkTransformMatrixKHR GetKhrMat();
		glm::mat4 GetMat4();
		glm::mat4 GetMat4(const glm::mat4& compareMat);
		glm::mat4 GetMat4(const Transform& transform, bool& changed);

		inline glm::vec3 GetTranslation() const { return m_Translation; }
		inline Quaternion GetRotation() const { return m_Rotation; }
		inline glm::vec3 GetScale() const { return m_Scale; }

		void SetTranslation(const glm::vec3& vec);
		void SetRotation(const glm::vec3& vec);
		void SetScale(const glm::vec3& vec);

		void AddTranslation(const glm::vec3& vec);
		void AddRotation(const glm::vec3& vec);
		void AddScale(const glm::vec3& vec);

	private:
		glm::mat4 ModelMatrix{1.0f};
		glm::vec3 m_Translation;
		glm::vec3 m_Scale;

		Quaternion m_Rotation{};
		glm::vec3 m_RightVec{ 1.0f, 0.0f, 0.0f };
		glm::vec3 UpVec{ 0.0f, -1.0f, 0.0f };
		glm::vec3 FrontVec{ 0.0f, 0.0f, -1.0f };
	};

}

