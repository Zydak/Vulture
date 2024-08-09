// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "Asset.h"
#include <unordered_map>
#include "AssetManager.h"
#include "Renderer/Model.h"

namespace Vulture
{

	AssetHandle::AssetHandle(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	AssetHandle::AssetHandle(const AssetHandle& other)
	{
		Init({ other.m_Handle });
	}

	AssetHandle::AssetHandle(AssetHandle&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Initialized	= std::move(other.m_Initialized);
		m_Handle		= std::move(other.m_Handle);

		other.Reset();
	}

	AssetHandle::~AssetHandle()
	{
		Destroy();
	}

	void AssetHandle::Init(const CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		m_Handle = createInfo.Handle;

		m_Initialized = true;
	}

	void AssetHandle::Destroy()
	{
		if (!m_Initialized)
			return;

		m_Initialized = false;
	}

	AssetHandle& AssetHandle::operator=(const AssetHandle& other)
	{
		Init({ other.m_Handle });
		return *this;
	}

	AssetHandle& AssetHandle::operator=(AssetHandle&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Initialized	= std::move(other.m_Initialized);
		m_Handle		= std::move(other.m_Handle);

		other.Reset();

		return *this;
	}

	Asset* AssetHandle::GetAsset() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return AssetManager::GetAsset(*this);
	}

	AssetType AssetHandle::GetAssetType() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return  AssetManager::GetAsset(*this)->GetAssetType();
	}

	Model* AssetHandle::GetModel() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return &reinterpret_cast<ModelAsset*>( AssetManager::GetAsset(*this))->Model;
	}

	Image* AssetHandle::GetImage() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return &reinterpret_cast<TextureAsset*>( AssetManager::GetAsset(*this))->Image;
	}

	bool AssetHandle::IsAssetValid() const
	{
		if (!m_Initialized)
			return false;

		return  AssetManager::IsAssetValid(*this);
	}

	bool AssetHandle::IsAssetLoaded() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return  AssetManager::IsAssetLoaded(*this);
	}

	void AssetHandle::Unload() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		 AssetManager::UnloadAsset(*this);
	}

	void AssetHandle::WaitToLoad() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		 AssetManager::WaitToLoad(*this);
	}

	size_t AssetHandle::Hash() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return (size_t)m_Handle;
	}

	void AssetHandle::Reset()
	{
		m_Handle = 0;
		m_Initialized = false;
	}

	bool AssetHandle::operator==(const AssetHandle& other) const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return m_Handle == other.m_Handle;
	}

	bool AssetHandle::operator==(uint64_t other) const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return m_Handle == other;
	}
}