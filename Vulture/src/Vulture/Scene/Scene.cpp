#include "pch.h"
#include "Scene.h"
#include "Entity.h"
#include "Components.h"


#include "../Renderer/AccelerationStructure.h"

namespace Vulture
{
	Scene::Scene(Ref<Window> window)
		: m_Window(window)
	{

	}

	Scene::~Scene()
	{
		if (m_AccelerationStructure)
			delete m_AccelerationStructure;
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

	Vulture::Entity Scene::CreateMesh(std::string filepath, Transform transform)
	{
		auto entity = CreateEntity();
		auto& mesh = entity.AddComponent<MeshComponent>();
		auto& transformComp = entity.AddComponent<TransformComponent>(transform);
		mesh.Mesh.CreateMesh(filepath);
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

	void Scene::CreateAtlas(const std::string& filepath)
	{
		m_Atlas = std::make_shared<TextureAtlas>(filepath);
	}

	void Scene::InitAccelerationStructure()
	{
		m_AccelerationStructure = new AccelerationStructure();
		m_AccelerationStructure->Init(*this);
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

	void Scene::AddFont(const std::string& fontFilepath, const std::string& fontName)
	{
		VL_CORE_ASSERT(m_FontAtlases.find(fontName) == m_FontAtlases.end(), "Font with this name already exists");
		m_FontAtlases[fontName] = std::make_shared<FontAtlas>(fontFilepath, fontName);
	}

	Ref<FontAtlas> Scene::GetFontAtlas(const std::string& fontName)
	{
		VL_CORE_ASSERT(m_FontAtlases.find(fontName) != m_FontAtlases.end(), "There is no such font");
		return m_FontAtlases[fontName];
	}

	std::vector<Entity> Scene::CheckCollisionsWith(Entity& mainEntity, const std::string nameToCheckAgainst)
	{
		std::vector<Entity> entitiesThatItCollidesWith;
		VL_CORE_ASSERT(mainEntity.HasComponent<ColliderComponent>() == true, "This entity has no collider!");
		ColliderComponent& mainCollider = mainEntity.GetComponent<ColliderComponent>();
		auto view = m_Registry.view<ColliderComponent>();
		for (auto& entity : view)
		{
			if (entity != mainEntity.GetHandle())
			{
				ColliderComponent& entityCollider = m_Registry.get<ColliderComponent>(entity);
				if (entityCollider.ColliderName == nameToCheckAgainst)
				{
					if (mainCollider.CheckCollision(entityCollider))
					{
						entitiesThatItCollidesWith.push_back({ entity, this });
					}
				}
			}
		}

		return entitiesThatItCollidesWith;
	}
}