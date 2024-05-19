#pragma once
#include "pch.h"
#include "../Renderer/Text.h"
#include "../Math/Transform.h"
#include "../Renderer/Model.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "Math/Quaternion.h"

#include "Entity.h"
#include "Asset/Asset.h"

namespace Vulture
{
	class ScriptInterface
	{
	public:
		ScriptInterface() = default;
		virtual ~ScriptInterface() {}
		virtual void OnCreate() = 0;
		virtual void OnDestroy() = 0;
		virtual void OnUpdate(double deltaTime) = 0;

		template<typename T>
		T& GetComponent()
		{
			return m_Entity.GetComponent<T>();
		}

		Entity m_Entity;
	};

	class ScriptComponent
	{
	public:

		~ScriptComponent();

		// TODO delete vector from here
		std::vector<ScriptInterface*> Scripts;

		template<typename T>
		void AddScript()
		{
			Scripts.push_back(new T());
		}

		void InitializeScripts();
		void UpdateScripts(double deltaTime);
		void DestroyScripts();

		inline std::vector<ScriptInterface*> GetScripts() const { return Scripts; }

		/**
		 * @brief Retrieves script at specified index.
		 *
		 * @return T* if specified T exists at scriptIndex, otherwise nullptr.
		 */
		template<typename T>
		T* GetScript(uint32_t scriptIndex)
		{
			T* returnVal;
			try
			{
				returnVal = dynamic_cast<T*>(Scripts.at(scriptIndex));
			}
			catch (const std::exception&)
			{
				returnVal = nullptr;
			}

			return returnVal;
		}

		inline uint32_t GetScriptCount() const { return (uint32_t)Scripts.size(); }
	};

	class SpriteComponent
	{
	public:
		glm::vec2 AtlasOffsets;

		SpriteComponent(const glm::vec2 atlasOffsets);

		void Draw();
	};
}