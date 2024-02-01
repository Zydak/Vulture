#pragma once

#include "pch.h"
#include "Vulkan/Image.h"

namespace Vulture
{
	class ObjectLoader
	{
	public:
		ObjectLoader() = delete;
		ObjectLoader(const ObjectLoader&) = delete;

		static Ref<Image> LoadTexture(const std::string& filepath);

	private:

		static std::unordered_map<std::string, Ref<Image>> m_LoadedTextures;
	};
}