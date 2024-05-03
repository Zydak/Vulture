#pragma once
#include "entt/entt.h"
#include "pch.h"
#include "../Renderer/Transform.h"
#include "../Renderer/FontAtlas.h"

#include "System.h"

namespace Vulture
{
	// forward declaration
	class Entity;
	class AccelerationStructure;
	class CameraComponent;
	class SkyboxComponent;
	class AssetManager;

	class Scene
	{
	public:
		void Init(Ref<Window> window, AssetManager* manager);
		void Destroy();

		Scene() = default;
		Scene(Ref<Window> window, AssetManager* manager);
		~Scene();

		// TODO add more creates, like skybox for example just don't use CreateEntity for everything
		Entity CreateEntity();
		void DestroyEntity(Entity& entity);
		Entity CreateCamera();

		Entity CreateSprite(const Transform& transform, const glm::vec2& tileOffset);

		Entity CreateSkybox(const std::string& filepath);

		Entity CreateModel(std::string filepath, Transform transform);

		void InitAccelerationStructure();
		Ref<AccelerationStructure> GetAccelerationStructure() { return m_AccelerationStructure; }

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

		void AddFont(const std::string& fontFilepath, const std::string& fontName);
		Ref<FontAtlas> GetFontAtlas(const std::string& fontName);

		std::vector<Entity> CheckCollisionsWith(Entity& entity, const std::string nameToCheckAgainst);

		inline entt::registry& GetRegistry() { return *m_Registry; }
		Ref<Window> GetWindow() const { return m_Window; }
		CameraComponent* GetMainCamera();
		Vulture::Entity GetMainCameraEntity();

		inline uint32_t GetModelCount() const { return m_ModelCount; }
		inline void ResetModelCount() { m_ModelCount = 0; }

	private:
		AssetManager* m_AssetManager;
		Ref<AccelerationStructure> m_AccelerationStructure = nullptr;
		Ref<Window> m_Window;
		Ref<entt::registry> m_Registry;
		std::vector<SystemInterface*> m_Systems;
		std::unordered_map<std::string, Ref<FontAtlas>> m_FontAtlases;
		uint32_t m_ModelCount = 0;

		bool m_Initialized = false;
	};

}