#pragma once
#include "Utility/Utility.h"
#include "Renderer/Model.h"
#include <future>

namespace Vulture
{
	class AssetManager;

	enum class AssetType
	{
		Model,
		Texture,
	};

	class Asset
	{
	public:
		virtual ~Asset() {};

		virtual AssetType GetAssetType() = 0;

		inline void SetValid(bool val) { m_Valid = val; }
		inline bool IsValid() const { return m_Valid; }
	protected:
		bool m_Valid = false;

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

		virtual AssetType GetAssetType() { return AssetType::Texture; }
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

		virtual AssetType GetAssetType() { return AssetType::Model; }
		Vulture::Model Model;
	};

	class AssetHandle
	{
	public:
		struct CreateInfo
		{
			uint64_t Handle;
			AssetManager* Manager;
		};

		AssetHandle() = default;
		AssetHandle(const CreateInfo& createInfo);
		~AssetHandle();

		void Init(const CreateInfo& createInfo);
		void Destroy();

		explicit AssetHandle(const AssetHandle& other);
		explicit AssetHandle(const AssetHandle&& other) noexcept = delete;
		AssetHandle& operator=(const AssetHandle& other);
		AssetHandle& operator=(const AssetHandle&& other) noexcept = delete;

		Ref<Asset> GetAsset() const;
		AssetType GetAssetType() const;
		Model* GetModel() const;
		Image* GetImage() const;
		bool IsAssetValid() const;
		bool IsAssetLoaded() const;

		void WaitToLoad() const;

		bool operator==(const AssetHandle& other) const;
		bool operator==(uint64_t other) const;
		size_t Hash() const;
	private:
		AssetManager* m_Manager;
		uint64_t m_Handle;

		bool m_Initialized = false;
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