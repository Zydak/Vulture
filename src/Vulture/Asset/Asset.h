// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "Utility/Utility.h"
#include <future>
#include "Vulkan/Image.h"
#include "Scene/Scene.h"
#include "Renderer/Mesh.h"

namespace Vulture
{
	class AssetManager;

	enum class AssetType
	{
		Mesh,
		Material,
		Model,
		Texture,
		Scene,
	};

	class Asset
	{
	public:
		virtual ~Asset() {};

		virtual AssetType GetAssetType() = 0;

		inline void SetValid(bool val) { Valid = val; }
		inline bool IsValid() const { return Valid; }
		inline void SetPath(const std::string& path) { Path = path; }
		inline std::string GetPath() { return Path; }
	protected:
		bool Valid = false;
		std::string Path = "";

		friend AssetManager;
	};

	class Material;

	class AssetHandle
	{
	public:
		struct CreateInfo
		{
			uint64_t Handle;
		};

		AssetHandle() = default;
		AssetHandle(const CreateInfo& createInfo);
		~AssetHandle();

		void Init(const CreateInfo& createInfo);
		void Destroy();

		AssetHandle(const AssetHandle& other);
		AssetHandle(AssetHandle&& other) noexcept;
		AssetHandle& operator=(const AssetHandle& other);
		AssetHandle& operator=(AssetHandle&& other) noexcept;

		Asset* GetAsset() const;
		AssetType GetAssetType() const;
		Mesh* GetMesh() const;
		Material* GetMaterial() const;
		Scene* GetScene() const;
		Image* GetImage() const;
		bool IsAssetValid() const;
		bool DoesHandleExist() const;
		bool IsAssetLoaded() const;

		inline bool IsInitialized() const { return m_Initialized; }

		void Unload() const;
		void WaitToLoad() const;

		bool operator==(const AssetHandle& other) const;
		bool operator==(uint64_t other) const;
		size_t Hash() const;
	private:
		uint64_t m_Handle = 0;

		bool m_Initialized = false;

		void Reset();
	};

	class MaterialProperties
	{
	public:
		glm::vec4 Color;
		glm::vec4 EmissiveColor;
		float Metallic = 0.0f;
		float Roughness = 1.0f;
		float SpecularTint = 0.0f;
		float SpecularStrength = 0.0f;

		float Ior = 1.5f;
		float Transparency = 0.0f;

		float Anisotropy = 0.0f;
	};

	class MaterialTextures
	{
	public:
		AssetHandle AlbedoTexture;
		AssetHandle NormalTexture;
		AssetHandle RoughnessTexture;
		AssetHandle MetallnessTexture;

		Vulture::DescriptorSet TexturesSet;

		void CreateSet();
	};

	class Material
	{
	public:
		MaterialProperties Properties;
		MaterialTextures Textures;

		std::string MaterialName;
	};

	class TextureAsset : public Asset
	{
	public:
		explicit TextureAsset(Image&& image) { Image = std::move(image); };
		~TextureAsset() {};
		explicit TextureAsset(const TextureAsset& other) = delete;
		TextureAsset& operator=(const TextureAsset& other) = delete;
		explicit TextureAsset(TextureAsset&& other) noexcept { Image = std::move(other.Image); }
		TextureAsset& operator=(TextureAsset&& other) noexcept { Image = std::move(other.Image); return *this; };

		virtual AssetType GetAssetType() override { return AssetType::Texture; }
		Vulture::Image Image;
	};

	class MeshAsset : public Asset
	{
	public:
		explicit MeshAsset(Mesh&& mesh) { Mesh = std::move(mesh); };
		~MeshAsset() {};
		explicit MeshAsset(const MeshAsset& other) = delete;
		MeshAsset& operator=(const MeshAsset& other) = delete;
		explicit MeshAsset(MeshAsset&& other) noexcept { Mesh = std::move(other.Mesh); }
		MeshAsset& operator=(MeshAsset&& other) noexcept { Mesh = std::move(other.Mesh); return *this; };

		virtual AssetType GetAssetType() override { return AssetType::Mesh; }
		Vulture::Mesh Mesh;
	};

	class MaterialAsset : public Asset
	{
	public:
		explicit MaterialAsset(Material&& material) { Material = std::move(material); };
		~MaterialAsset() {};
		explicit MaterialAsset(const MaterialAsset& other) = delete;
		MaterialAsset& operator=(const MaterialAsset& other) = delete;
		explicit MaterialAsset(MaterialAsset&& other) noexcept { Material = std::move(other.Material); }
		MaterialAsset& operator=(MaterialAsset&& other) noexcept { Material = std::move(other.Material); return *this; };

		virtual AssetType GetAssetType() override { return AssetType::Material; }
		Vulture::Material Material;
	};

	class ModelAsset : public Asset
	{
	public:
		ModelAsset() = default;
		~ModelAsset() = default;
		explicit ModelAsset(const ModelAsset& other) = delete;
		ModelAsset& operator=(const ModelAsset& other) = delete;
		ModelAsset(ModelAsset&& other) noexcept 
		{ 
			Meshes = std::move(other.Meshes);
			MeshNames = std::move(other.MeshNames);
			Materials = std::move(other.Materials);
			MeshTransfrorms = std::move(other.MeshTransfrorms);
		}
		ModelAsset& operator=(ModelAsset&& other) noexcept 
		{ 
			Meshes = std::move(other.Meshes); 
			Materials = std::move(other.Materials);
			MeshNames = std::move(other.MeshNames);
			MeshTransfrorms = std::move(other.MeshTransfrorms);

			return *this;
		};

		virtual AssetType GetAssetType() override { return AssetType::Model; }
		
		std::vector<AssetHandle> Meshes;
		std::vector<std::string> MeshNames;
		std::vector<glm::mat4> MeshTransfrorms;
		std::vector<AssetHandle> Materials;

		void CreateEntities(Vulture::Scene* outScene);
	};

	class SceneAsset : public Asset
	{
	public:
		SceneAsset(Vulture::Scene&& scene) { Scene = std::move(scene); };

		explicit SceneAsset(const SceneAsset& other) = delete;
		explicit SceneAsset(SceneAsset&& other) noexcept { Scene = std::move(other.Scene); };
		SceneAsset& operator=(const SceneAsset& other) = delete;
		SceneAsset& operator=(SceneAsset&& other) noexcept { Scene = std::move(other.Scene); return *this; };

		virtual AssetType GetAssetType() override { return AssetType::Scene; }
		Vulture::Scene Scene;
	};
}

namespace std
{
	template<>
	struct std::hash<Vulture::AssetHandle>
	{
		std::size_t operator()(const Vulture::AssetHandle& k) const
		{
			return k.Hash();
		}
	};
}