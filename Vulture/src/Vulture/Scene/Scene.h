#pragma once
#include "entt/entt.h"
#include "pch.h"
#include "../Renderer/Transform.h"
#include "../Renderer/TextureAtlas.h"
#include "../Renderer/FontAtlas.h"

#include "System.h"

namespace Vulture
{
	// forward declaration
	class Entity;
	class AccelerationStructure;
	class CameraComponent;

	class Scene
	{
	public:
		Scene(Ref<Window> window);
		~Scene();

		Entity CreateEntity();
		void DestroyEntity(Entity& entity);
		Entity CreateCamera();

		Entity CreateSprite(const Transform& transform, const glm::vec2& tileOffset);
		void CreateStaticSprite(const Transform& transform, const glm::vec2& tileOffset);

		Entity CreateModel(std::string filepath, Transform transform);

		void CreateAtlas(const std::string& filepath);

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

		inline entt::registry& GetRegistry() { return m_Registry; }
		inline Ref<TextureAtlas> GetAtlas() const { return m_Atlas; }
		inline uint32_t GetTextureCount() const { return m_TextureCount; }
		Ref<Window> GetWindow() const { return m_Window; }
		CameraComponent* GetMainCamera(Entity* entity = nullptr);

		inline uint32_t GetVertexCount() const { return m_VertexCount; }
		inline uint32_t GetIndexCount() const { return m_IndexCount; }

	private:
		Ref<AccelerationStructure> m_AccelerationStructure = nullptr;
		Ref<Window> m_Window;
		entt::registry m_Registry;
		Ref<TextureAtlas> m_Atlas;
		std::vector<SystemInterface*> m_Systems;
		std::unordered_map<std::string, Ref<FontAtlas>> m_FontAtlases;
		uint32_t m_TextureCount = 0;


		// Just for fun
		uint32_t m_VertexCount = 0;
		uint32_t m_IndexCount = 0;
	};

}