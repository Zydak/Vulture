#pragma once
#include "pch.h"


#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/gtc/matrix_transform.hpp"

namespace Vulture
{

	class Transform
	{
	public:
		Transform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
		~Transform();

		glm::mat4 GetMat4() 
		{
			ModelMatrix = glm::mat4{ 1.0f };
			ModelMatrix = glm::translate(ModelMatrix, m_Translation);
			ModelMatrix = glm::rotate(ModelMatrix, glm::radians(m_Rotation.x), glm::vec3(0.0f, 1.0f, 0.0f));
			ModelMatrix = glm::rotate(ModelMatrix, glm::radians(m_Rotation.y), glm::vec3(1.0f, 0.0f, 0.0f));
			ModelMatrix = glm::rotate(ModelMatrix, glm::radians(m_Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
			ModelMatrix = glm::scale(ModelMatrix, m_Scale);

			return ModelMatrix;
		};

		glm::mat4 GetMat4(const glm::mat4& compareMat)
		{
			if (ModelMatrix != compareMat)
			{
				ModelMatrix = glm::mat4{ 1.0f };
				ModelMatrix = glm::translate(ModelMatrix, m_Translation);
				ModelMatrix = glm::rotate(ModelMatrix, glm::radians(m_Rotation.x), glm::vec3(0.0f, 1.0f, 0.0f));
				ModelMatrix = glm::rotate(ModelMatrix, glm::radians(m_Rotation.y), glm::vec3(1.0f, 0.0f, 0.0f));
				ModelMatrix = glm::rotate(ModelMatrix, glm::radians(m_Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
				ModelMatrix = glm::scale(ModelMatrix, m_Scale);
			}

			return ModelMatrix;
		};

		glm::mat4 GetMat4(const Transform& transform, bool& changed)
		{
			changed = false;
			if (m_Translation != transform.GetTranslation() || m_Rotation != transform.GetRotation() || m_Scale != transform.GetScale())
			{
				ModelMatrix = glm::mat4{ 1.0f };
				ModelMatrix = glm::translate(ModelMatrix, m_Translation);
				ModelMatrix = glm::rotate(ModelMatrix, glm::radians(m_Rotation.x), glm::vec3(0.0f, 1.0f, 0.0f));
				ModelMatrix = glm::rotate(ModelMatrix, glm::radians(m_Rotation.y), glm::vec3(1.0f, 0.0f, 0.0f));
				ModelMatrix = glm::rotate(ModelMatrix, glm::radians(m_Rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
				ModelMatrix = glm::scale(ModelMatrix, m_Scale);
				changed = true;
			}
			return ModelMatrix;
		};

		inline glm::vec3 GetTranslation() const { return m_Translation; }
		inline glm::vec3 GetRotation() const { return m_Rotation; }
		inline glm::vec3 GetScale() const { return m_Scale; }

		void SetTranslation(const glm::vec3& vec) 
		{ 
			m_Translation = vec;
		}
		void SetRotation(const glm::vec3& vec) 
		{ 
			m_Rotation = vec;
		}
		void SetScale(const glm::vec3& vec) 
		{ 
			m_Scale = vec;
		}

		void AddTranslation(const glm::vec3& vec) 
		{ 
			m_Translation += vec;
		}
		void AddRotation(const glm::vec3& vec) 
		{ 
			m_Rotation += vec;
		}
		void AddScale(const glm::vec3& vec) 
		{ 
			m_Scale += vec;
		}

	private:
		glm::mat4 ModelMatrix{1.0f};
		glm::vec3 m_Translation;
		glm::vec3 m_Rotation;
		glm::vec3 m_Scale;
	};

}

