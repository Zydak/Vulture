#pragma once
#include "Asset.h"
#include "Utility/Utility.h"

#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <utility>

namespace Vulture
{
	class AssetManager
	{
	public:
		struct CreateInfo
		{
			uint32_t ThreadCount = 1;
		};

		AssetManager() = default;
		AssetManager(const CreateInfo& createInfo);
		~AssetManager();

		void Init(const CreateInfo& createInfo);
		void Destroy();

		explicit AssetManager(const AssetManager& other);
		explicit AssetManager(AssetManager&& other) noexcept;
		AssetManager& operator=(const AssetManager& other);
		AssetManager& operator=(AssetManager&& other) noexcept;

		Ref<Asset> GetAsset(const AssetHandle& handle);
		bool IsAssetValid(const AssetHandle& handle) const;

		void WaitToLoad(const AssetHandle& handle) const;
		bool IsAssetLoaded(const AssetHandle& handle) const;

		AssetHandle LoadAsset(const std::string& path);
		void UnloadAsset(const AssetHandle& handle);
	private:
		std::unordered_map<AssetHandle, std::shared_future<Ref<Asset>>> m_Assets;
		ThreadPool m_ThreadPool;

		bool m_Initialized = false;
	};
}