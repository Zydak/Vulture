#pragma once
#include "entt/entt.h"
#include "pch.h"
#include "../Renderer/Transform.h"
#include "../Renderer/TextureAtlas.h"

#include "System.h"

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

		template<typename T>
		void AddSystem()
		{
			m_Systems.emplace_back(new T());
		}

		void InitSystems();
		void DestroySystems();
		void UpdateSystems(double deltaTime);

		std::vector<Entity> CheckCollisionsWith(Entity& entity);

		inline entt::registry& GetRegistry() { return m_Registry; }
		inline std::shared_ptr<TextureAtlas> GetAtlas() { return m_Atlas; }

	private:
		float m_TilingSize = 0;
		entt::registry m_Registry;
		Ref<TextureAtlas> m_Atlas;
		std::vector<SystemInterface*> m_Systems;
	};

}