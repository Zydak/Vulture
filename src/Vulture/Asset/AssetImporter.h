#pragma once
#include "Vulkan/Image.h"
#include "Renderer/Model.h"

namespace Vulture
{
	class AssetManager;

	class AssetImporter
	{
	public:
		static Image ImportTexture(const std::string& path, bool HDR);
		static Model ImportModel(const std::string& path, AssetManager* assetManager);
	private:
	};

}