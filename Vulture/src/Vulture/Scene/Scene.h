#pragma once
#include "entt/entt.h"
#include "pch.h"
#include "../Renderer/Transform.h"
#include "../Renderer/TextureAtlas.h"

namespace Vulture
{
	// forward declaration
	class Entity;

	class Scene
	{
	public:
		Scene();
		~Scene();

		Entity CreateEntity();
		Entity CreateSprite(const Transform& transform, const glm::vec2& tileOffset);
		void CreateStaticSprite(const Transform& transform, const glm::vec2& tileOffset);
		Entity CreateCamera();
		void DestroyEntity(Entity& entity);

		void CreateAtlas(const std::string& filepath);

		void InitScripts();
		void DestroyScripts();
		void UpdateScripts(double deltaTime);

		inline entt::registry& GetRegistry() { return m_Registry; }
		inline std::shared_ptr<TextureAtlas> GetAtlas() { return m_Atlas; }

	private:
		entt::registry m_Registry;
		Ref<TextureAtlas> m_Atlas;
	};

}