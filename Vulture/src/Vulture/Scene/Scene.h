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
		Entity CreateSprite(const Transform& transform, const std::string& textureFilepath);
		Entity CreateCamera();
		void DestroyEntity(Entity& entity);

		void AddTextureToAtlas(const std::string& filepath);
		void PackAtlas();

		inline entt::registry& GetRegistry() { return m_Registry; }
		inline TextureAtlas& GetAtlas() { return m_Atlas; }

	private:
		entt::registry m_Registry;
		TextureAtlas m_Atlas;
	};

}