#pragma once
#include "pch.h"
#include "glm/glm.hpp"
#include "../Renderer/Transform.h"

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
}