// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"
#include "Asset/AssetManager.h"

#include "../Renderer/AccelerationStructure.h"

namespace Vulture
{
	void Scene::Init(Ref<Window> window)
	{
		if (m_Initialized)
			Destroy();

		m_Registry = std::make_shared<entt::registry>();
		m_Window = window;
		m_Initialized = true;
	}

	void Scene::Destroy()
	{
		if (!m_Initialized)
			return;

		Reset();
	}

	Scene::Scene(Ref<Window> window)
	{
		Init(window);
	}

	Scene::Scene(Scene&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Window = std::move(other.m_Window);
		m_Registry = std::move(other.m_Registry);
		m_Systems = std::move(other.m_Systems);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();
	}

	Scene& Scene::operator=(Scene&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Window = std::move(other.m_Window);
		m_Registry = std::move(other.m_Registry);
		m_Systems = std::move(other.m_Systems);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	Scene::~Scene()
	{
		Destroy();
	}

	Entity Scene::CreateEntity()
	{
		Entity entity = { m_Registry->create(), this };
		return entity;
	}

	void Scene::DestroyEntity(Entity& entity)
	{
		m_Registry->destroy(entity);
	}

	void Scene::InitScripts()
	{
		auto view = m_Registry->view<ScriptComponent>();
		for (auto entity : view)
		{
			ScriptComponent& scriptComponent = view.get<ScriptComponent>(entity);
			for (int i = 0; i < scriptComponent.Scripts.size(); i++)
			{
				scriptComponent.Scripts[i]->m_Entity = { entity, this };
				scriptComponent.Scripts[i]->OnCreate();
			}
		}
	}

	void Scene::DestroyScripts()
	{
		auto view = m_Registry->view<ScriptComponent>();
		for (auto entity : view)
		{
			ScriptComponent& scriptComponent = view.get<ScriptComponent>(entity);
			for (int i = 0; i < scriptComponent.Scripts.size(); i++)
			{
				scriptComponent.Scripts[i]->OnDestroy();
			}
		}
	}

	void Scene::UpdateScripts(double deltaTime)
	{
		auto view = m_Registry->view<ScriptComponent>();
		for (auto entity : view)
		{
			ScriptComponent& scriptComponent = view.get<ScriptComponent>(entity);
			for (int i = 0; i < scriptComponent.Scripts.size(); i++)
			{
				scriptComponent.Scripts[i]->OnUpdate(deltaTime);
			}
		}
	}

	void Scene::InitSystems()
	{
		for (int i = 0; i < m_Systems.size(); i++)
		{
			m_Systems[i]->OnCreate();
		}
	}

	void Scene::DestroySystems()
	{
		for (int i = 0; i < m_Systems.size(); i++)
		{
			m_Systems[i]->OnDestroy();
		}
	}

	void Scene::UpdateSystems(double deltaTime)
	{
		for (int i = 0; i < m_Systems.size(); i++)
		{
			m_Systems[i]->OnUpdate(deltaTime);
		}
	}

	void Scene::Reset()
	{
		m_Window = nullptr;
		m_Registry = nullptr;
		m_Systems.clear();
		m_Initialized = false;
	}

}