#pragma once
#include "pch.h"
#include "Vulkan/Image.h"
#include "Vulkan/DescriptorSet.h"

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
		void Init(const std::string& filepath);
		void Destroy();

		TextureAtlas() = default;
		TextureAtlas(const std::string& filepath);
		~TextureAtlas();

		glm::vec2 GetTextureOffset(const glm::vec2& tileOffset) const;
		Ref<DescriptorSet> GetAtlasDescriptorSet() const;

		void SetTiling(int tiling);

	private:
		Ref<Image> m_AtlasTexture;
		Ref<DescriptorSet> m_AtlasDescriptorSet;
		int m_TilingSize = 32;

		bool m_Initialized = false;
	};

}