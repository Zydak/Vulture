#pragma once

#include "pch.h"
#include "Vulkan/Image.h"
#include "Renderer/Model.h"

namespace Vulture
{
	class AssetManager
	{
	public:
		AssetManager() = delete;
		AssetManager(const AssetManager&) = delete;

		static void Destroy();

		static Ref<Image> LoadTexture(const std::string& filepath);
		static Ref<Image> CreateTexture(const glm::vec4& color, const Image::CreateInfo& info);
		static Ref<Image> CreateTexture(const Image::CreateInfo& info);

		static Ref<Model> LoadModel(const std::string& filepath);

		static void ProgressDestroyQueue();

		static void Cleanup();

	private:

		struct Resource
		{
			Ref<Image> ImagePtr;
			Ref<Model> ModelPtr;

			int FramesToDelete;
		};

		static std::unordered_map<uint64_t, Ref<Image>> s_Textures;
		static std::unordered_map<uint64_t, Ref<Model>> s_Models;
		static std::vector<Resource> s_DestroyQueue;
	};
}