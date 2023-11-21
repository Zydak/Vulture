#pragma once
#include "pch.h"
#include "glm/glm.hpp"
#include "../Renderer/Transform.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/gtc/matrix_transform.hpp"

namespace Vulture
{

	class ScriptInterface
	{
	public:
		virtual void OnCreate() = 0;
		virtual void OnDestroy() = 0;
		virtual void OnUpdate(double deltaTime) = 0;
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

	class CameraComponent
	{
	public:
		glm::vec3 Translation{0.0f};
		glm::mat4 ProjMat{1.0f};
		glm::mat4 ViewMat{1.0f};
		bool Main = false;

		CameraComponent() 
		{

		};

		void SetOrthographicMatrix(glm::vec4 leftRightBottomTop, float _near, float _far, float aspectRatio)
		{
			ProjMat = glm::ortho(leftRightBottomTop.x, leftRightBottomTop.y, leftRightBottomTop.z, leftRightBottomTop.w, _near, _far);
		}

		void SetPerspectiveMatrix(float fov, float aspectRatio, float _near, float _far)
		{
			ProjMat = glm::perspective(glm::radians(fov), aspectRatio, _near, _far);
		}

		void UpdateViewMatrix()
		{
			ViewMat = glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			ViewMat = glm::translate(ViewMat, Translation);
		}

		glm::mat4 GetViewProj()
		{
			return ProjMat * ViewMat;
		}
	};
}