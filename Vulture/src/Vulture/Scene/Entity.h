#pragma once
#include "pch.h"
#include <entt/entt.h>
#include "Scene.h"

namespace Vulture
{

	class Entity
	{
	public:
		Entity() {};
		Entity(entt::entity handle, Scene* scene);
		~Entity();

		inline entt::entity GetHandle() { return m_Handle; }

		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			VL_CORE_ASSERT(!HasComponent<T>(), "Entity already has this component!");
			T& component = m_Scene->GetRegistry().emplace<T>(m_Handle, std::forward<Args>(args)...);
			return component;
		}

		template<typename T>
		bool HasComponent()
		{
			return m_Scene->GetRegistry().any_of<T>(m_Handle);
		}

		template<typename T>
		T& GetComponent()
		{
			return m_Scene->GetRegistry().get<T>(m_Handle);
		}

		inline Scene* GetScene() { return m_Scene; }

		operator entt::entity() const { return m_Handle; }
		operator bool() const{ return (uint32_t)m_Handle != 0; }

	private:
		entt::entity m_Handle;
		Scene* m_Scene;
	};

}