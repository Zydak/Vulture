#include "pch.h"
#include "AssetImporter.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "AssetManager.h"

namespace Vulture
{
	Image AssetImporter::ImportTexture(const std::string& path, bool HDR)
	{
		int texChannels;
		stbi_set_flip_vertically_on_load(!HDR);
		int sizeX, sizeY;
		void* pixels;
		if (HDR)
		{
			pixels = stbi_loadf(path.c_str(), &sizeX, &sizeY, &texChannels, STBI_rgb_alpha);
		}
		else
		{
			pixels = stbi_load(path.c_str(), &sizeX, &sizeY, &texChannels, STBI_rgb_alpha);
		}

		std::filesystem::path cwd = std::filesystem::current_path();
		VL_CORE_ASSERT(pixels, "failed to load texture image! Path: {0}, Current working directory: {1}", path, cwd.string());
		uint64_t sizeOfPixel = HDR ? sizeof(float) * 4 : sizeof(uint8_t) * 4;
		VkDeviceSize imageSize = (uint64_t)sizeX * (uint64_t)sizeY * sizeOfPixel;

		Image::CreateInfo info{};
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.Format = HDR ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;
		info.Height = sizeY;
		info.Width = sizeX;
		info.LayerCount = 1;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Data = (void*)pixels;
		info.HDR = HDR;
		info.MipMapCount = glm::min(5, (int)glm::floor(glm::log2((float)glm::max(sizeX, sizeY))));
		Image image(info);

		stbi_image_free(pixels);

		return Image(std::move(image));
	}

	Model AssetImporter::ImportModel(const std::string& path, AssetManager* assetManager)
	{
		return Model(path, assetManager);
	}

}