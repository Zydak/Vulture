#include "pch.h"

#include "ObjectLoader.h"

namespace Vulture
{

	Ref<Image> ObjectLoader::LoadTexture(const std::string& filepath)
	{
		if (m_LoadedTextures.find(filepath) != m_LoadedTextures.end())
		{
			Ref<Image> image = m_LoadedTextures[filepath];
			return image;
		}
		else
		{
			m_LoadedTextures[filepath] = std::make_shared<Image>(filepath);
			Ref<Image> image = m_LoadedTextures[filepath];
			return image;
		}
	}

	std::unordered_map<std::string, Ref<Image>> ObjectLoader::m_LoadedTextures;

}