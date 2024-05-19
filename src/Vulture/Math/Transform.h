#pragma once
#include "pch.h"

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/gtc/matrix_transform.hpp"

#include "Quaternion.h"

namespace Vulture
{
	class Transform
	{
	public:
		Transform() = default;
		Transform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);

		void Init(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
		void Destroy();

		~Transform();
		Transform(const Transform& other);
		Transform(Transform&& other) noexcept;
		Transform& operator=(const Transform& other);
		Transform& operator=(Transform&& other) noexcept;

		VkTransformMatrixKHR GetKhrMat();
		// Maybe add ComputeMat4() function and make getter const?
		glm::mat4 GetMat4();

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
		glm::mat4 m_ModelMatrix{1.0f};
		glm::vec3 m_Translation{};
		glm::vec3 m_Scale{};

		Quaternion m_Rotation{};

		bool m_Initialized = false;
		void Reset();
	};

}

