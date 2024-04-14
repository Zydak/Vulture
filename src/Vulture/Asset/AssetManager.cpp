#include "pch.h"
#include "AssetManager.h"
#include "AssetImporter.h"
#include <future>

namespace Vulture
{

	AssetManager::AssetManager(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	AssetManager::AssetManager(const AssetManager& other)
	{
		// TODO
	}

	AssetManager::AssetManager(AssetManager&& other) noexcept
	{
		// TODO
	}

	AssetManager& AssetManager::operator=(AssetManager&& other) noexcept
	{
		// TODO
		return *this;
	}

	AssetManager& AssetManager::operator=(const AssetManager& other)
	{
		// TODO
		return *this;
	}

	AssetManager::~AssetManager()
	{

	}

	void AssetManager::Init(const CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		m_ThreadPool.Init({ createInfo.ThreadCount });

		m_Initialized = true;
	}

	void AssetManager::Destroy()
	{
		m_ThreadPool.Destroy();
		m_Assets.clear();
		m_Initialized = false;
	}

	Ref<Asset> AssetManager::GetAsset(const AssetHandle& handle)
	{
		auto iter = m_Assets.find(handle);
		VL_CORE_ASSERT(iter != m_Assets.end(), "There is no such handle!");

		return iter->second.get();
	}

	bool AssetManager::IsAssetValid(const AssetHandle& handle) const
	{
		auto iter = m_Assets.find(handle);
		VL_CORE_ASSERT(iter != m_Assets.end(), "There is no such handle!");
		const std::shared_future<Ref<Asset>>& future = iter->second;

		return future.get()->m_Valid;
	}

	void AssetManager::WaitToLoad(const AssetHandle& handle) const
	{
		auto iter = m_Assets.find(handle);
		VL_CORE_ASSERT(iter != m_Assets.end(), "There is no such handle!");
		const std::shared_future<Ref<Asset>>& future = iter->second;

		future.wait();
	}

	bool AssetManager::IsAssetLoaded(const AssetHandle& handle) const
	{
		auto iter = m_Assets.find(handle);
		VL_CORE_ASSERT(iter != m_Assets.end(), "There is no such handle!");
		const std::shared_future<Ref<Asset>>& future = iter->second;

		return future.wait_for(std::chrono::duration<float>(0)) == std::future_status::ready;
	}

	AssetHandle AssetManager::LoadAsset(const std::string& path)
	{
		std::hash<std::string> hash;
		AssetHandle handle(AssetHandle::CreateInfo{hash(path), this});
		if (m_Assets.contains(handle))
		{
			// Asset with this path is already loaded
			return AssetHandle(handle);
		}

		size_t dotPos = path.find_last_of('.');
		VL_CORE_ASSERT(dotPos != std::string::npos, "Failed to get file extension! Path: {}", path);
		std::string extension = path.substr(dotPos, path.size() - dotPos);

		std::shared_ptr<std::promise<Ref<Asset>>> promise = std::make_shared<std::promise<Ref<Asset>>>();

		m_Assets[handle] = promise->get_future();
		if (extension == ".png" || extension == ".jpg")
		{
			m_ThreadPool.PushTask([](const std::string& path, std::shared_ptr<std::promise<Ref<Asset>>> promise)
				{
					Ref<Asset> asset = std::make_shared<TextureAsset>(std::move(AssetImporter::ImportTexture(path, false)));
					asset->SetValid(true);
					promise->set_value(asset);
				}, path, promise);
		}
		else if (extension == ".gltf" || extension == ".obj")
		{
			m_ThreadPool.PushTask([](const std::string& path, std::shared_ptr<std::promise<Ref<Asset>>> promise)
				{
					Ref<Asset> asset = std::make_shared<ModelAsset>(std::move(AssetImporter::ImportModel(path)));
					asset->SetValid(true);
					promise->set_value(asset);
				}, path, promise);
		}
		else { VL_CORE_ASSERT(false, "Extension not supported! Extension: {}", extension); }

		return AssetHandle(handle);
	}

	void AssetManager::UnloadAsset(const AssetHandle& handle)
	{

	}

}