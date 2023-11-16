#pragma once
#include "entt/entt.h"
#include "pch.h"

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
		void DestroyEntity(Entity& entity);

		inline entt::registry& GetRegistry() { return m_Registry; }

	private:
		entt::registry m_Registry;
	};

}