#include "pch.h"

#include "AssetManager.h"
#include "Vulkan/Swapchain.h"
#include <string>
#include <unordered_map>

#include <random>

namespace Vulture
{
	static std::random_device s_RandomDevice;
	static std::mt19937_64 s_Engine(s_RandomDevice());
	static std::uniform_int_distribution<uint64_t> s_Distribution;

	void AssetManager::Destroy()
	{
		for (auto& image : s_Textures)
		{
			image.second.reset();
		}
		for (auto& model : s_Textures)
		{
			model.second.reset();
		}

		s_Textures.clear();
		s_Models.clear();
		s_DestroyQueue.clear();
	}

	Ref<Image> AssetManager::LoadTexture(const std::string& filepath)
	{
		uint64_t hash = std::hash<std::string>{}(filepath);
		if (s_Textures.find(hash) != s_Textures.end())
		{
			// Found
			Ref<Image> image = s_Textures[hash];
			return image;
		}
		else
		{
			uint64_t uuid = hash;

			s_Textures[uuid] = std::make_shared<Image>(filepath);
			Ref<Image> image = s_Textures[uuid];
			return image;
		}
	}

	Ref<Image> AssetManager::CreateTexture(const glm::vec4& color, const Image::CreateInfo& info)
	{
		uint64_t uuid = s_Distribution(s_Engine);

		s_Textures[uuid] = std::make_shared<Image>(color, info);
		Ref<Image> image = s_Textures[uuid];
		return image;
	}

	Ref<Image> AssetManager::CreateTexture(const Image::CreateInfo& info)
	{
		uint64_t uuid = s_Distribution(s_Engine);

		s_Textures[uuid] = std::make_shared<Image>(info);
		Ref<Image> image = s_Textures[uuid];
		return image;
	}

	Ref<Model> AssetManager::LoadModel(const std::string& filepath)
	{
		uint64_t hash = std::hash<std::string>{}(filepath);
		if (s_Models.find(hash) != s_Models.end())
		{
			// Found
			Ref<Model> model = s_Models[hash];
			return model;
		}
		else
		{
			uint64_t uuid = hash;

			s_Models[uuid] = std::make_shared<Model>(filepath);
			Ref<Model> model = s_Models[uuid];
			return model;
		}
	}

	void AssetManager::ProgressDestroyQueue()
	{
		for (int i = 0; i < s_DestroyQueue.size(); i++)
		{
			if (s_DestroyQueue[i].FramesToDelete < 0)
			{
				s_DestroyQueue.erase(s_DestroyQueue.begin() + i);
			}
			s_DestroyQueue[i].FramesToDelete--;
		}
	}

	void AssetManager::Cleanup()
	{
		int i = 0;
		for (auto image : s_Textures)
		{
			i++;
			if (image.second.use_count() == 1)
			{
				AssetManager::Resource res;
				res.FramesToDelete = MAX_FRAMES_IN_FLIGHT;
				res.ImagePtr = image.second;
				s_DestroyQueue.push_back(res);

				s_Textures.erase(image.first);
			}
		}

		for (auto model : s_Models)
		{
			if (model.second.use_count() == 1)
			{
				AssetManager::Resource res;
				res.FramesToDelete = MAX_FRAMES_IN_FLIGHT;
				res.ModelPtr = model.second;
				s_DestroyQueue.push_back(res);

				s_Models.erase(model.first);
			}
		}
	}

	std::unordered_map<uint64_t, Ref<Image>> AssetManager::s_Textures;
	std::unordered_map<uint64_t, Ref<Model>> AssetManager::s_Models;

	std::vector<AssetManager::Resource> AssetManager::s_DestroyQueue;

}