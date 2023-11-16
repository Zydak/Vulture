#pragma once
#include "pch.h"
#include "glm/glm.hpp"
#include "Vulkan/Image.h"

namespace Vulture
{

	class TextureAtlas
	{
	public:
		TextureAtlas();
		~TextureAtlas();

		void AddTexture(const std::string& filepath);
		//void RemoveTexture(const std::string filepath);
		void SetTiling(int tiling);
		void SetAtlasSize(glm::vec2 atlasSize);
		void PackAtlas();

	private:
		std::unordered_map<std::string, glm::vec2> m_Textures;
		std::vector<std::pair<std::string, Scope<Image>>> m_Images;
		Ref<Image> m_AtlasTexture;
		int m_TilingSize = 32;
		glm::vec2 m_LastOffset = {0.0f, 0.0f};
		glm::vec2 m_AtlasSize = {100.0f, 100.0f};
		int m_BiggestYInRow = 0;
	};

}