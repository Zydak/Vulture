#include "pch.h"
#include "Asset.h"
#include <unordered_map>
#include "AssetManager.h"

#include "glm/gtx/matrix_decompose.hpp"

#include "Renderer/Renderer.h"

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

	Mesh* AssetHandle::GetMesh() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return &dynamic_cast<MeshAsset*>(AssetManager::GetAsset(*this))->Mesh;
	}

	Material* AssetHandle::GetMaterial() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return &dynamic_cast<MaterialAsset*>(AssetManager::GetAsset(*this))->Material;
	}

	Scene* AssetHandle::GetScene() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return &dynamic_cast<SceneAsset*>(AssetManager::GetAsset(*this))->Scene;
	}

	Image* AssetHandle::GetImage() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return &dynamic_cast<TextureAsset*>( AssetManager::GetAsset(*this))->Image;
	}

	bool AssetHandle::IsAssetValid() const
	{
		if (!m_Initialized)
			return false;

		return AssetManager::IsAssetValid(*this);
	}

	bool AssetHandle::DoesHandleExist() const
	{
		if (!m_Initialized)
			return false;

		return AssetManager::DoesHandleExist(*this);
	}

	bool AssetHandle::IsAssetLoaded() const
	{
		VL_CORE_ASSERT(m_Initialized, "Handle is not Initialized!");

		return AssetManager::IsAssetLoaded(*this);
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

	void ModelAsset::CreateEntities(Vulture::Scene* outScene, bool addMaterials)
	{
		for (int i = 0; i < Meshes.size(); i++)
		{
			Vulture::Entity entity = outScene->CreateEntity();

			Vulture::MeshComponent* meshComp = &entity.AddComponent<MeshComponent>();
			Vulture::Mesh* mesh = Meshes[i].GetMesh();
			meshComp->AssetHandle = Meshes[i];

			auto& nameComp = entity.AddComponent<NameComponent>();
			nameComp.Name = MeshNames[i];

			if (addMaterials)
			{
				auto& materialComp = entity.AddComponent<MaterialComponent>();
				materialComp.AssetHandle = Materials[i];
			}

			glm::mat4 modelMatrix{ 1.0f };
			glm::vec3 translation{};
			glm::vec3 scale{};
			glm::vec3 skew; // don't care
			glm::vec4 perspective; // don't care

			glm::quat rotation{};

			glm::decompose(MeshTransfrorms[i], scale, rotation, translation, skew, perspective);

			Vulture::Transform transform;
			transform.SetRotation(rotation);
			transform.SetTranslation(translation);
			transform.SetScale(scale);

			auto& transformComp = entity.AddComponent<TransformComponent>();
			transformComp.Transform = std::move(transform);

			// This automatically waits for textures
			Materials[i].GetMaterial()->Textures.CreateSet();

			meshComp->AssetHandle.WaitToLoad();
		}
	}

	void MaterialTextures::CreateSet()
	{
		if (TexturesSet.IsInitialized())
			return;

		Vulture::DescriptorSetLayout::Binding bin1{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		Vulture::DescriptorSetLayout::Binding bin2{ 1, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		Vulture::DescriptorSetLayout::Binding bin3{ 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		Vulture::DescriptorSetLayout::Binding bin4{ 3, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };

		TexturesSet.Init(&Vulture::Renderer::GetDescriptorPool(), { bin1, bin2, bin3, bin4 });

		AlbedoTexture.WaitToLoad();
		TexturesSet.AddImageSampler(
			0,
			{ Vulture::Renderer::GetLinearRepeatSampler().GetSamplerHandle(),
			AlbedoTexture.GetImage()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		);
		NormalTexture.WaitToLoad();
		TexturesSet.AddImageSampler(
			1,
			{ Vulture::Renderer::GetLinearRepeatSampler().GetSamplerHandle(),
			NormalTexture.GetImage()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		);
		RoughnessTexture.WaitToLoad();
		TexturesSet.AddImageSampler(
			2,
			{ Vulture::Renderer::GetLinearRepeatSampler().GetSamplerHandle(),
			RoughnessTexture.GetImage()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		);
		MetallnessTexture.WaitToLoad();
		TexturesSet.AddImageSampler(
			3,
			{ Vulture::Renderer::GetLinearRepeatSampler().GetSamplerHandle(),
			MetallnessTexture.GetImage()->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
		);

		TexturesSet.Build();
	}

}