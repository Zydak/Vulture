// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

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
	class SerializeBaseClass
	{
	public:
		virtual std::vector<char> Serialize() { return {}; }
		virtual void Deserialize(const std::vector<char>& bytes) {  }
	};

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

	class ScriptComponent : public SerializeBaseClass
	{
	public:
		ScriptComponent() = default;

		~ScriptComponent();

		ScriptComponent(ScriptComponent&& other) noexcept;

		// TODO delete vector from here
		std::vector<ScriptInterface*> Scripts;

		// This vector is holding indexes into g_ScriptNames vec so that the overall structure takes less memory.
		// And since it's used only during serialization, memory indirection doesn't really affect performance
		std::vector<uint32_t> ScriptClassesNames;

		template<typename T>
		void AddScript()
		{
			Scripts.push_back(new T());

			std::string name = typeid(T).name();
			if (name.find("class ") != std::string::npos)
				name = name.substr(6, name.size() - 6);

			ScriptClassesNames.push_back(g_ScriptNames.size());
			g_ScriptNames.push_back(name);
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

		std::vector<char> Serialize() override;
		void Deserialize(const std::vector<char>& bytes) override;


		// It's used to save names for serialization and deserialization of script components
		static std::vector<std::string> g_ScriptNames;
	};
}