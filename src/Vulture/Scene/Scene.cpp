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
		m_Window.reset();
		m_Registry.reset();
		m_Initialized = false;
	}

	Scene::Scene(Ref<Window> window)
	{
		Init(window);
	}

	Scene::~Scene()
	{
		if (m_Initialized)
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
}