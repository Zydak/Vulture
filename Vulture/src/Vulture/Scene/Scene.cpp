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

	Entity Scene::CreateSprite(const Transform& transform, const std::string& textureFilepath)
	{
		Entity entity = CreateEntity();
		entity.AddComponent<TransformComponent>(transform);
		glm::vec2& offset = m_Atlas.GetTextureOffset(textureFilepath);
		entity.AddComponent<SpriteComponent>(offset);
		return entity;
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

	void Scene::PackAtlas()
	{
		m_Atlas.PackAtlas();
	}

	void Scene::AddTextureToAtlas(const std::string& filepath)
	{
		m_Atlas.AddTexture(filepath);
	}

}