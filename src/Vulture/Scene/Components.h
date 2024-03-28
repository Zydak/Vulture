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

		~ScriptComponent();

		// TODO delete vector from here
		std::vector<ScriptInterface*> Scripts;

		template<typename T>
		void AddScript()
		{
			Scripts.push_back(new T());
		}

		void InitializeScripts();
		void UpdateScripts(double deltaTime);
		void DestroyScripts();

		inline std::vector<ScriptInterface*> GetScripts() const { return Scripts; }

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
	};

	class SpriteComponent
	{
	public:
		glm::vec2 AtlasOffsets;

		SpriteComponent(const glm::vec2 atlasOffsets);

		void Draw();
	};

	class TransformComponent
	{
	public:
		Transform transform;

		TransformComponent(const Transform& transform);
		TransformComponent(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale);
	};

	class CameraComponent
	{
	public:
		Quaternion Rotation{};
		glm::vec3 Translation{ 0.0f };
		glm::mat4 ProjMat{1.0f};
		glm::mat4 ViewMat{1.0f};
		bool Main = false;
		float Zoom = 1.0f;
		float FOV = 45.0f;
		float AspectRatio = 1.0f;

		void Reset();

		void SetOrthographicMatrix(glm::vec4 leftRightBottomTop, float _near, float _far, float aspectRatio);
		void SetPerspectiveMatrix(float fov, float aspectRatio, float _near, float _far);
		void SetZoom(float zoom);

		void UpdateViewMatrix();
		void UpdateViewMatrixCustom(const glm::mat4 mat);

		void AddRotation(const glm::vec3& vec);
		void AddPitch(float pitch);
		void AddYaw(float yaw);
		void AddRoll(float roll);

		glm::mat4 GetViewProj();
		inline const glm::vec3 GetFrontVec() const { return Rotation.GetFrontVec(); }
		inline const glm::vec3 GetRightVec() const { return Rotation.GetRightVec(); }
		inline const glm::vec3 GetUpVec() const { return Rotation.GetUpVec(); }
		inline const float GetAspectRatio() const { return AspectRatio; }
	};

	class ColliderComponent
	{
	public:
		Vulture::Entity Entity;
		std::string ColliderName;

		ColliderComponent(const Vulture::Entity& entity, const std::string name);

		bool CheckCollision(ColliderComponent& otherCollider);
	};

	class TextComponent
	{
	public:
		Vulture::Text Text;
		TextComponent(const Text::CreateInfo& createInfo);

		void ChangeText(const std::string& string, float kerningOffset = 0.0f);
	};

	class ModelComponent
	{
	public:
		Ref<Vulture::Model> Model; // todo: don't use Ref here

		ModelComponent(const std::string filepath);

		~ModelComponent() = default;

	private:
	};

	class SkyboxComponent
	{
	public:
		Ref<Vulture::Image> SkyboxImage; // todo: don't use Ref here

		SkyboxComponent(const std::string& filepath);

		~SkyboxComponent() = default;
	};
}