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
		Init({ other.m_Handle, other.m_Manager });
	}

	AssetHandle::AssetHandle(AssetHandle&& other) noexcept
	{
		m_Initialized = true;
		m_Handle = other.m_Handle;
		m_Manager = other.m_Manager;

		other.m_Initialized = false;
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
		m_Manager = createInfo.Manager;

		m_Initialized = true;
	}

	void AssetHandle::Destroy()
	{
		m_Initialized = false;
	}

	AssetHandle& AssetHandle::operator=(const AssetHandle& other)
	{
		Init({ other.m_Handle, other.m_Manager });
		return *this;
	}

	AssetHandle& AssetHandle::operator=(AssetHandle&& other) noexcept
	{
		m_Initialized = true;
		m_Handle = other.m_Handle;
		m_Manager = other.m_Manager;

		other.m_Initialized = false;

		return *this;
	}

	Asset* AssetHandle::GetAsset() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return m_Manager->GetAsset(*this);
	}

	AssetType AssetHandle::GetAssetType() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return m_Manager->GetAsset(*this)->GetAssetType();
	}

	Model* AssetHandle::GetModel() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return &reinterpret_cast<ModelAsset*>(m_Manager->GetAsset(*this))->Model;
	}

	Image* AssetHandle::GetImage() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return &reinterpret_cast<TextureAsset*>(m_Manager->GetAsset(*this))->Image;
	}

	bool AssetHandle::IsAssetValid() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return m_Manager->IsAssetValid(*this);
	}

	bool AssetHandle::IsAssetLoaded() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return m_Manager->IsAssetLoaded(*this);
	}

	void AssetHandle::Unload() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		m_Manager->UnloadAsset(*this);
	}

	void AssetHandle::WaitToLoad() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		m_Manager->WaitToLoad(*this);
	}

	size_t AssetHandle::Hash() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return (size_t)m_Handle;
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