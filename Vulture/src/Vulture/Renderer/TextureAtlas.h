#pragma once
#include "pch.h"
#include "Vulkan/Image.h"
#include "Vulkan/Uniform.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/glm.hpp"

namespace Vulture
{
	struct AtlasInfoBuffer
	{
		glm::vec4 TilingSize;
	};

	class TextureAtlas
	{
	public:
		TextureAtlas(const std::string& filepath);
		~TextureAtlas();

		glm::vec2 GetTextureOffset(const glm::vec2& tileOffset);
		Ref<Uniform> GetAtlasUniform();

		void SetTiling(int tiling);

	private:
		Ref<Image> m_AtlasTexture;
		Ref<Uniform> m_AtlasUniform;
		int m_TilingSize = 32;
	};

}