#pragma once
#include "pch.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"

namespace Vulture
{
	class Serializer
	{
	public:
		template <typename... Components>
		static void SerializeScene(Scene* scene, const std::string& filepath)
		{
			std::vector<char> bytesOut;

			auto& reg = scene->GetRegistry();

			reg.each([&](entt::entity entity) 
			{
				std::tuple<Components*...> tuple = reg.try_get<Components...>(entity);

				uint32_t componentsCount = 0;
				TupleNonNullMemberCount(tuple, componentsCount);
				if (componentsCount != 0)
				{
					std::vector<char> bytesSize = Vulture::Bytes::ToBytes(&componentsCount, 4);
					bytesOut.insert(bytesOut.end(), bytesSize.begin(), bytesSize.end()); // number of components on the entity

					SerializeComponents(tuple, bytesOut);
				}
			});

			uint64_t size = (uint64_t)bytesOut.size() + 8; // + 8 to account for this 8 size bytes
			std::vector<char> bytesSize = Vulture::Bytes::ToBytes((char*)&size, 8);
			bytesOut.insert(bytesOut.begin(), bytesSize.begin(), bytesSize.end()); // first 8 bytes are overall size of the file

			std::ofstream ofstream;
			ofstream.open(filepath, std::ios_base::binary | std::ios_base::trunc);

			ofstream.write(bytesOut.data(), bytesOut.size());
			ofstream.flush();

			ofstream.close();
		}

		template <typename... Components>
		static void DeserializeScene(const std::string& filepath, Scene* outScene)
		{
			std::ifstream ifstream(filepath, std::ios_base::binary);

			uint64_t size = 0;
			ifstream.read((char*)&size, 8);
			ifstream.seekg(0);

			std::vector<char> bytes(size);
			ifstream.read(bytes.data(), size);

			ifstream.seekg(8); // skip first 8 bytes of size data
			uint64_t currentPos = 8;
			while (currentPos < size)
			{
				// Get the component count on this entity
				uint32_t componentCount = 0;
				ifstream.read((char*)&componentCount, 4);
				currentPos += 4;

				Vulture::Entity entity = outScene->CreateEntity();

				for (uint64_t i = 0; i < componentCount; i++)
				{
					// Get the component name
					std::string compName;
					while (true)
					{
						char ch;
						ifstream.read(&ch, 1);
						currentPos += 1;

						if (ch == '\0')
						{
							break;
						}

						compName.push_back(ch);
					}

					// Create a component from a registered constructor
					void* component = CreateRegisteredClass(compName);

					// Read 8 bytes of data size
					uint64_t componentDataSize = 0;
					ifstream.read((char*)&componentDataSize, 8);
					currentPos += 8;

					// read the data
					std::vector<char> componentByteData(componentDataSize);
					ifstream.read(componentByteData.data(), componentDataSize);
					currentPos += componentDataSize;

					// Push component onto the entity
					DeduceObjectTypeAndAddToEntity<Components...>(component, compName, entity, componentByteData);

					delete component;
				}
			}
		}

#define REGISTER_CLASS_IN_SERIALIZER(className) Vulture::Serializer::RegisterClass<className>(#className)

		template<typename T>
		static void RegisterClass(const std::string& className)
		{
			s_ReflectionMap[className] = []() { return (void*)(new T()); };
		}

		static void* CreateRegisteredClass(const std::string& className)
		{
			VL_CORE_ASSERT(s_ReflectionMap.find(className) != s_ReflectionMap.end(), "class {} wasn't registered!", className);

			return s_ReflectionMap[className]();
		}

	private:

		template<typename T>
		static void PushComponent(T&& comp, Vulture::Entity& entity)
		{
			entity.AddComponent<T>(std::move(comp));
		}

		template<typename T>
		static T* TryCast(void* obj, const std::string& compName)
		{
			// Check and parse the name
			std::string name = typeid(T).name();
			if (name.find("class ") != std::string::npos)
				name = name.substr(6, name.size() - 6);

			if (name.find(" * __ptr64") != std::string::npos)
				name = name.substr(0, name.size() - 10);

			if (name.find(" * __ptr32") != std::string::npos)
				name = name.substr(0, name.size() - 10);

			if (name.find("Vulture::") != std::string::npos)
				name = name.substr(9, name.size() - 9);

			// If the name matches that of a component then return it
			if (name == compName)
				return (T*)(obj);
			else
				return nullptr;

			//return dynamic_cast<T*>(obj);
		}

		template<typename... T>
		static void DeduceObjectTypeAndAddToEntity(void* obj, const std::string& compName, Vulture::Entity& entity, const std::vector<char>& deserializedData)
		{
			if constexpr (sizeof...(T) == 0)
				return;
			else
			{
				// Try casting the obj into one of the components given in T
				// It will fill the tuple with nullptrs except for the original obj type
				std::tuple<T*...> tuple = { TryCast<T>(obj, compName)... };

				IterateTupleAndAddComponent(tuple, entity, deserializedData);
			}
		}

		template<size_t I = 0, typename... T>
		constexpr static void IterateTupleAndAddComponent(const std::tuple<T...>& tuple, Vulture::Entity& entity, const std::vector<char>& deserializedData)
		{
			// Iterate the tuple and check if the component != nullptr. If true then add the component to the entity
			// otherwise keep iterating

			if constexpr (I == sizeof...(T))
				return;
			else
			{
				auto comp = std::get<I>(tuple);
				if (comp != nullptr)
				{
					if (!deserializedData.empty())
						comp->Deserialize(deserializedData);

					PushComponent(std::move(*comp), entity);
					return;
				}

				IterateTupleAndAddComponent<I + 1>(tuple, entity, deserializedData);
			}
		}

		template<size_t I = 0, typename... T>
		constexpr static void SerializeComponents(const std::tuple<T...>& tuple, std::vector<char>& bytesOut)
		{
			if constexpr(I == sizeof...(T))
				return;
			else
			{
				// Tuple is filled with nullptr except for the components that the entity contains
				// so iterate over the tuple and check if they are nullptr or not
				auto comp = std::get<I>(tuple);
				if (comp != nullptr)
					SerializeComponent(comp, bytesOut);

				SerializeComponents<I + 1>(tuple, bytesOut);
			}
		}

		template<size_t I = 0, typename... T>
		constexpr static void TupleNonNullMemberCount(const std::tuple<T...>& tuple, uint32_t& outSize)
		{
			if constexpr (I == sizeof...(T))
				return;
			else
			{
				auto obj = std::get<I>(tuple);
				if (obj != nullptr) 
					outSize += 1;

				TupleNonNullMemberCount<I + 1>(tuple, outSize);
			}
		}

		template<typename T>
		static void SerializeComponent(T component, std::vector<char>& bytesOut)
		{
			std::string name = typeid(T).name();
			if (name.find("class ") != std::string::npos)
				name = name.substr(6, name.size() - 6);

			if (name.find(" * __ptr64") != std::string::npos)
				name = name.substr(0, name.size() - 10);

			if (name.find(" * __ptr32") != std::string::npos)
				name = name.substr(0, name.size() - 10);

			if (name.find("Vulture::") != std::string::npos)
				name = name.substr(9, name.size() - 9);

			std::vector<char> nameBytes;
			for (int i = 0; i < name.size(); i++)
			{
				nameBytes.push_back(name[i]);
			}
			nameBytes.push_back('\0');

			std::vector<char> componentBytes = component->Serialize();

			std::vector<char> combinedBytes;
			combinedBytes.reserve(nameBytes.size() + 8 + componentBytes.size());

			// First the name of the component class
			combinedBytes.insert(combinedBytes.end(), nameBytes.begin(), nameBytes.end());

			// Size of the component data bytes
			uint64_t size = (uint64_t)componentBytes.size();
			std::vector<char> bytesSize = Vulture::Bytes::ToBytes((char*)&size, 8);
			combinedBytes.insert(combinedBytes.end(), bytesSize.begin(), bytesSize.end()); // first 8 bytes are overall size of the file

			// Component Data
			combinedBytes.insert(combinedBytes.end(), componentBytes.begin(), componentBytes.end());

			// Push the data out
			bytesOut.reserve(bytesOut.size() + combinedBytes.size());
			bytesOut.insert(bytesOut.end(), combinedBytes.begin(), combinedBytes.end());
		}

		static inline std::unordered_map<std::string, std::function<void* ()>> s_ReflectionMap;
	};

}