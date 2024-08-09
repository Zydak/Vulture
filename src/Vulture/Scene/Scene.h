// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "entt/entt.h"
#include "pch.h"
#include "../Math/Transform.h"
#include "Vulkan/Window.h"
#include "Utility/Utility.h"

#include "System.h"

namespace Vulture
{
	// forward declaration
	class Entity;
	class AssetManager;

	class Scene
	{
	public:
		void Init(Ref<Window> window);
		void Destroy();

		Scene() = default;
		Scene(Ref<Window> window);
		~Scene();

		Scene(const Scene& other) = delete;
		Scene& operator=(const Scene& other) = delete;
		Scene(Scene&& other) noexcept;
		Scene& operator=(Scene&& other) noexcept;

		Entity CreateEntity();
		void DestroyEntity(Entity& entity);

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

		inline entt::registry& GetRegistry() { return *m_Registry; }
		inline Ref<Window> GetWindow() const { return m_Window; }

		inline bool IsInitialized() const { return m_Initialized; }

	private:
		Ref<Window> m_Window = nullptr;
		Ref<entt::registry> m_Registry = nullptr;
		std::vector<SystemInterface*> m_Systems;

		bool m_Initialized = false;

		void Reset();
	};

}