#pragma once
#include "Utility/Utility.h"
#include <future>
#include "Vulkan/Image.h"

namespace Vulture
{
	class AssetManager;
	class Model;

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

		virtual AssetType GetAssetType() { return AssetType::Texture; }
		Image Image;
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
		explicit AssetHandle(AssetHandle&& other) noexcept;
		AssetHandle& operator=(const AssetHandle& other);
		AssetHandle& operator=(AssetHandle&& other) noexcept;

		Asset* GetAsset() const;
		AssetType GetAssetType() const;
		Model* GetModel() const;
		Image* GetImage() const;
		bool IsAssetValid() const;
		bool IsAssetLoaded() const;

		void Unload() const;
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