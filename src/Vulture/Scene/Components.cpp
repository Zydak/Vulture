// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "Components.h"
#include "Renderer/Renderer.h"

#include "Asset/Serializer.h"

namespace Vulture
{
	void ScriptComponent::InitializeScripts()
	{
		for (auto& script : Scripts)
		{
			script->OnCreate();
		}
	}

	void ScriptComponent::UpdateScripts(double deltaTime)
	{
		for (auto& script : Scripts)
		{
			script->OnUpdate(deltaTime);
		}
	}

	void ScriptComponent::DestroyScripts()
	{
		for (auto& script : Scripts)
		{
			script->OnDestroy();
		}
	}

	std::vector<char> ScriptComponent::Serialize()
	{
		std::vector<char> bytes;

		for (int i = 0; i < ScriptClassesNames.size(); i++)
		{
			for (int j = 0; j < g_ScriptNames[ScriptClassesNames[i]].size(); j++)
			{
				bytes.push_back(g_ScriptNames[ScriptClassesNames[i]][j]);
			}
		}

		bytes.push_back('\0');

		return bytes;
	}

	void ScriptComponent::Deserialize(const std::vector<char>& bytes)
	{
		std::vector<std::string> scriptClassNames(1);

		if (bytes[0] == '\0') // No scripts attached
			return;

		for (int i = 0; i < bytes.size(); i++)
		{
			if (bytes[i] == '\0')
			{
				scriptClassNames.emplace_back("");
			}

			scriptClassNames[scriptClassNames.size() - 1].push_back(bytes[i]);
		}

		scriptClassNames.erase(scriptClassNames.end() - 1); // last index is always empty due to for loop above

		std::vector<uint32_t> classNamesIndexes;
		for (int i = 0; i < scriptClassNames.size(); i++)
		{
			for (int j = 0; j < g_ScriptNames.size(); j++)
			{
				if (scriptClassNames[i] == g_ScriptNames[j])
					classNamesIndexes.push_back(j); // Index found
			}

			// Index not found
			if (classNamesIndexes.size() != i)
			{
				classNamesIndexes.push_back((uint32_t)g_ScriptNames.size());
				g_ScriptNames.push_back(scriptClassNames[i]);
			}
		}

		ScriptClassesNames = classNamesIndexes;

		for (int i = 0; i < scriptClassNames.size(); i++)
		{
			SerializeBaseClass* baseClass = Serializer::CreateRegisteredClassRawPtr(scriptClassNames[i]);
			ScriptInterface* scInterface = dynamic_cast<ScriptInterface*>(baseClass);

			VL_CORE_ASSERT(scInterface != nullptr, "Script doesn't inherit from script interface!");

			Scripts.push_back(scInterface);
		}
	}

	ScriptComponent::~ScriptComponent()
	{
		for (auto& script : Scripts)
		{
			delete script;
		}
	}

	ScriptComponent::ScriptComponent(ScriptComponent&& other) noexcept
	{
		Scripts = std::move(other.Scripts);
	}

	std::vector<std::string> ScriptComponent::g_ScriptNames;
}