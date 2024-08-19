// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "Utility/Utility.h"
#include <future>
#include "Vulkan/Image.h"
#include "Renderer/Model.h"
#include "Scene/Scene.h"

namespace Vulture
{
	class AssetManager;
	class Model;

	enum class AssetType
	{
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
		Image Image;
	};

	class ModelAsset : public Asset
	{
	public:
		ModelAsset(Model&& model) { Model = std::move(model); };

		explicit ModelAsset(const ModelAsset& other) = delete;
		explicit ModelAsset(ModelAsset&& other) noexcept { Model = std::move(other.Model); };
		ModelAsset& operator=(const ModelAsset& other) = delete;
		ModelAsset& operator=(ModelAsset&& other) noexcept { Model = std::move(other.Model); return *this; };

		virtual AssetType GetAssetType() override { return AssetType::Model; }
		Vulture::Model Model;
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

		explicit AssetHandle(const AssetHandle& other);
		explicit AssetHandle(AssetHandle&& other) noexcept;
		AssetHandle& operator=(const AssetHandle& other);
		AssetHandle& operator=(AssetHandle&& other) noexcept;

		Asset* GetAsset() const;
		AssetType GetAssetType() const;
		Model* GetModel() const;
		Scene* GetScene() const;
		Image* GetImage() const;
		bool IsAssetValid() const;
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