// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "pch.h"

#include "Asset.h"
#include "Utility/Utility.h"

#include "AssetImporter.h"

namespace Vulture
{
	struct AssetWithFuture
	{
		std::shared_future<void> Future;
		Scope<Asset> Asset;
	};

	class AssetManager
	{
	public:
		struct CreateInfo
		{
			uint32_t ThreadCount = 1;
		};

		AssetManager() = delete;

		static void Init(const CreateInfo& createInfo);
		static void Destroy();

		static Asset* GetAsset(const AssetHandle& handle);
		static bool IsAssetValid(const AssetHandle& handle);
		static bool DoesHandleExist(const AssetHandle& handle);

		static void WaitToLoad(const AssetHandle& handle);
		static bool IsAssetLoaded(const AssetHandle& handle);
		static AssetHandle LoadAsset(std::string path);
		static AssetHandle AddAsset(const std::string path, std::unique_ptr<Asset>&& asset);
		static void UnloadAsset(const AssetHandle& handle);

		static inline bool IsInitialized() { return s_Initialized; }

		// T is list of types of components which to deserialize
		template<typename... T>
		static AssetHandle LoadSceneAsset(std::string path)
		{
			std::hash<std::string> hash;
			AssetHandle handle(AssetHandle::CreateInfo{ hash(path) });
			if (s_Assets.contains(handle))
			{
				// Asset with this path is already loaded
				return AssetHandle(handle);
			}

			VL_CORE_TRACE("Loading asset: {}", path);

			size_t dotPos = path.find_last_of('.');
			VL_CORE_ASSERT(dotPos != std::string::npos, "Failed to get file extension! Path: {}", path);
			std::string extension = path.substr(dotPos, path.size() - dotPos);

			std::shared_ptr<std::promise<void>> promise = std::make_shared<std::promise<void>>();

			std::unique_lock<std::mutex> lock(s_AssetsMutex);
			s_Assets[handle] = { promise->get_future(), nullptr };
			lock.unlock();

			s_ThreadPool.PushTask([](std::string path, std::shared_ptr<std::promise<void>> promise, AssetHandle handle)
				{
					Scope<Asset> asset = std::make_unique<SceneAsset>(std::move(AssetImporter::ImportScene<T...>(path)));
					asset->SetValid(true);
					asset->SetPath(path);
			
					std::unique_lock<std::mutex> lock(s_AssetsMutex);
					s_Assets[handle].Asset = std::move(asset);
					lock.unlock();
			
					promise->set_value();
				}, path, promise, handle);

			return AssetHandle(handle);
		}
	private:

		inline static std::unordered_map<AssetHandle, AssetWithFuture> s_Assets;
		inline static ThreadPool s_ThreadPool;
		inline static std::mutex s_AssetsMutex;

		inline static bool s_Initialized = false;

		friend class AssetImporter;
	};
}