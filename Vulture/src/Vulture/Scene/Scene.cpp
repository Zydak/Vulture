#include "pch.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"

namespace Vulture
{

	Scene::Scene()
	{

	}

	Scene::~Scene()
	{

	}

	Entity Scene::CreateEntity()
	{
		Entity entity = { m_Registry.create(), this };
		return entity;
	}

	/*
	* @brief Creates a sprite entity by combining a TransformComponent and a SpriteComponent.
	* The TransformComponent is initialized with the provided 'transform', and the SpriteComponent is
	* initialized with the texture offset obtained from the entity's 'tileOffset' within the texture atlas.
	*
	* @param transform The transformation information for the sprite.
	* @param tileOffset The offset within the texture atlas to determine the sprite's texture.
	* @return The created sprite entity.
	*/
	Entity Scene::CreateSprite(const Transform& transform, const glm::vec2& tileOffset)
	{
		Entity entity = CreateEntity();
		entity.AddComponent<TransformComponent>(transform);
		glm::vec2 offset = m_Atlas->GetTextureOffset(tileOffset);
		entity.AddComponent<SpriteComponent>(offset);
		return entity;
	}

	/*
	* @brief Creates a static sprite entity by combining a StaticTransformComponent and a SpriteComponent.
	* The StaticTransformComponent is initialized with the provided 'transform', and the SpriteComponent is
	* initialized with the texture offset obtained from the entity's 'tileOffset' within the texture atlas.
	* Static sprites are suitable for objects that do not change their position, optimizing
	* performance by avoiding unnecessary recalculations.
	*
	* @param transform The transformation information for the static sprite.
	* @param tileOffset The offset within the texture atlas to determine the static sprite's texture.
	*/
	void Scene::CreateStaticSprite(const Transform& transform, const glm::vec2& tileOffset)
	{
		Entity entity = CreateEntity();
		entity.AddComponent<StaticTransformComponent>(transform);
		glm::vec2 offset = m_Atlas->GetTextureOffset(tileOffset);
		entity.AddComponent<SpriteComponent>(offset);
	}

	Vulture::Entity Scene::CreateCamera()
	{
		static bool firstTimeCallingThisFunc = true;
		Entity entity = CreateEntity();
		entity.AddComponent<CameraComponent>();

		if (firstTimeCallingThisFunc)
			entity.GetComponent<CameraComponent>().Main = true;

		return entity;
	}

	void Scene::DestroyEntity(Entity& entity)
	{
		m_Registry.destroy(entity);
	}

	void Scene::CreateAtlas(const std::string& filepath)
	{
		m_Atlas = std::make_shared<TextureAtlas>(filepath);
	}

	void Scene::InitScripts()
	{
		auto view = m_Registry.view<ScriptComponent>();
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
		auto view = m_Registry.view<ScriptComponent>();
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
		auto view = m_Registry.view<ScriptComponent>();
		for (auto entity : view)
		{
			ScriptComponent& scriptComponent = view.get<ScriptComponent>(entity);
			for (int i = 0; i < scriptComponent.Scripts.size(); i++)
			{
				scriptComponent.Scripts[i]->OnUpdate(deltaTime);
			}
		}
	}

}