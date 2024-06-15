#pragma once
#include "Asset.h"
#include "Utility/Utility.h"

#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <utility>

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

		static void WaitToLoad(const AssetHandle& handle);
		static bool IsAssetLoaded(const AssetHandle& handle);

		static AssetHandle LoadAsset(const std::string& path);
		static void UnloadAsset(const AssetHandle& handle);

		static inline bool IsInitialized() { return s_Initialized; }
	private:
		static std::unordered_map<AssetHandle, AssetWithFuture> s_Assets;
		static ThreadPool s_ThreadPool;
		static std::mutex s_AssetsMutex;

		static bool s_Initialized;
	};
}