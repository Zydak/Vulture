#include "pch.h"
#include "Components.h"
#include "Renderer/Renderer.h"

// TODO: Move definitions here
namespace Vulture
{
	SkyboxComponent::SkyboxComponent(const std::string filepath)
	{
		Ref<Image> tempImage = std::make_shared<Image>(filepath);

		ImageInfo imageInfo = {};
		imageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageInfo.height = 1024;
		imageInfo.width = 1024;
		imageInfo.layerCount = 6;
		imageInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		imageInfo.samplerInfo = {};
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.type = ImageType::Cubemap;
		imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // TODO delte sampled
		SkyboxImage = std::make_shared<Image>(imageInfo);

		Renderer::EnvMapToCubemapPass(tempImage, SkyboxImage);
	}

}