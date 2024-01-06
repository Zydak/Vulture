#pragma once
#include "pch.h"
#include "../Renderer/Text.h"
#include "../Renderer/Transform.h"
#include "../Renderer/Model.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "Math/Quaternion.h"

#include "Entity.h"


namespace Vulture
{
	class ScriptInterface
	{
	public:
		ScriptInterface() = default;
		virtual ~ScriptInterface() {}
		virtual void OnCreate() = 0;
		virtual void OnDestroy() = 0;
		virtual void OnUpdate(double deltaTime) = 0;

		template<typename T>
		T& GetComponent()
		{
			return m_Entity.GetComponent<T>();
		}

		Entity m_Entity;
	};

	class ScriptComponent
	{
	public:
		// TODO delete vector from here
		std::vector<ScriptInterface*> Scripts;

		template<typename T>
		void AddScript()
		{
			Scripts.push_back(new T());
		}

		void InitializeScripts()
		{
			for (auto& script : Scripts)
			{
				script->OnCreate();
			}
		}

		void UpdateScripts(double deltaTime)
		{
			for (auto& script : Scripts)
			{
				script->OnUpdate(deltaTime);
			}
		}

		void DestroyScripts()
		{
			for (auto& script : Scripts)
			{
				script->OnDestroy();
			}
		}

		std::vector<ScriptInterface*> GetScripts()
		{
			return Scripts;
		}

		/**
		 * @brief Retrieves script at specified index.
		 *
		 * @return T* if specified T exists at scriptIndex, otherwise nullptr.
		 */
		template<typename T>
		T* GetScript(uint32_t scriptIndex)
		{
			T* returnVal;
			try
			{
				returnVal = dynamic_cast<T*>(Scripts.at(scriptIndex));
			}
			catch (const std::exception&)
			{
				returnVal = nullptr;
			}

			return returnVal;
		}

		inline uint32_t GetScriptCount() const { return (uint32_t)Scripts.size(); }

		~ScriptComponent()
		{
			for (auto& script : Scripts)
			{
				delete script;
			}
		}
	};

	class SpriteComponent
	{
	public:
		glm::vec2 AtlasOffsets;

		SpriteComponent(const glm::vec2 atlasOffsets)
			: AtlasOffsets(atlasOffsets)
		{

		}

		void Draw()
		{

		}
	};

	class TransformComponent
	{
	public:
		Transform transform;

		TransformComponent(const Transform& transform)
			: transform(transform)
		{

		}

		TransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
			: transform({translation, rotation, scale})
		{

		}
	};

	class StaticTransformComponent
	{
	public:
		Transform transform;

		StaticTransformComponent(const Transform& transform)
			: transform(transform)
		{

		}

		StaticTransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
			: transform({ translation, rotation, scale })
		{

		}
	};

	class CameraComponent
	{
	private:
		Quaternion Rotation{};

	public:
		glm::vec3 Translation{ 0.0f };
		glm::mat4 ProjMat{1.0f};
		glm::mat4 ViewMat{1.0f};
		bool Main = false;
		float Zoom = 1.0f;

		CameraComponent() 
		{

		};

		void SetOrthographicMatrix(glm::vec4 leftRightBottomTop, float _near, float _far, float aspectRatio)
		{
			ProjMat = glm::ortho(leftRightBottomTop.x * aspectRatio * Zoom, leftRightBottomTop.y * aspectRatio * Zoom, leftRightBottomTop.z * Zoom, leftRightBottomTop.w * Zoom, _near, _far);
		}

		void SetPerspectiveMatrix(float fov, float aspectRatio, float _near, float _far)
		{
			ProjMat = glm::perspective(glm::radians(fov), aspectRatio, _near, _far);
		}

		void SetZoom(float zoom)
		{
			Zoom = zoom;
		}

		void UpdateViewMatrix()
		{
			ViewMat = Rotation.GetMat4();
			ViewMat = glm::translate(ViewMat, Translation);
		}

		void UpdateViewMatrixCustom(const glm::mat4 mat)
		{
			ViewMat = mat;
		}

		glm::mat4 GetViewProj()
		{
			return ProjMat * ViewMat;
		}

		void AddRotation(const glm::vec3& vec)
		{
			Rotation.AddAngles(vec);
		}

		void AddPitch(float pitch)
		{
			Rotation.AddPitch(pitch);
		}

		void AddYaw(float yaw)
		{
			Rotation.AddYaw(yaw);
		}

		void AddRoll(float roll)
		{
			Rotation.AddRoll(roll);
		}

		inline const glm::vec3 GetFrontVec() const { return Rotation.GetFrontVec(); }
		inline const glm::vec3 GetRightVec() const { return Rotation.GetRightVec(); }
		inline const glm::vec3 GetUpVec() const { return Rotation.GetUpVec(); }
	};

	class ColliderComponent
	{
	public:
		Vulture::Entity Entity;
		std::string ColliderName;

		ColliderComponent(const Vulture::Entity& entity, const std::string name)
			: Entity(entity), ColliderName(name)
		{
			
		}

		bool CheckCollision(ColliderComponent& otherCollider)
		{
			TransformComponent transform = Entity.GetComponent<TransformComponent>().transform;
			TransformComponent otherTransform = otherCollider.Entity.GetComponent<TransformComponent>().transform;

			glm::vec2 position = transform.transform.GetTranslation() - transform.transform.GetScale();
			glm::vec2 size = transform.transform.GetScale() * 2.0f;

			glm::vec2 otherPosition = otherTransform.transform.GetTranslation() - otherTransform.transform.GetScale();
			glm::vec2 otherSize = otherTransform.transform.GetScale() * 2.0f;

			if (position.x < otherPosition.x + otherSize.x &&
				position.x + size.x > otherPosition.x &&
				position.y < otherPosition.y + otherSize.y &&
				position.y + size.x > otherPosition.y)
			{
				return true;
			}
			return false;
		}
	};

	class TextComponent
	{
	public:
		Vulture::Text Text;
		TextComponent(const std::string& text, Ref<FontAtlas> font, const glm::vec4& color, int maxLetterCount = 0, bool resizable = false, float kerningOffset = 0.0f)
			: Text(text, font, color, kerningOffset, maxLetterCount, resizable)
		{

		}

		void ChangeText(const std::string& string, float kerningOffset = 0.0f)
		{
			Text.ChangeText(string, kerningOffset);
		}
	};

	class ModelComponent
	{
	public:
		Vulture::Model Model;

		ModelComponent(const std::string filepath)
			: Model(filepath)
		{

		}

		~ModelComponent() = default;

	private:
	};
}