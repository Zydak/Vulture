// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

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
		static Model ImportModel(const std::string& path);
	private:
	};

}