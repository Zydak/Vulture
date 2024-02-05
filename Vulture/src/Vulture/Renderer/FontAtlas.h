#pragma once

#include "pch.h"
#undef INFINITE

#include "msdf-atlas-gen/GlyphGeometry.h"
#include "msdf-atlas-gen/FontGeometry.h"

#include "Vulkan/Image.h"
#include "Vulkan/DescriptorSet.h"

namespace Vulture
{
	// TODO: use asset manager
	class FontAtlas
	{
	public:
		FontAtlas(const std::string& filepath, const std::string& fontName);

		Ref<Image> GetAtlasTexture() const { return m_AtlasTexture; }
		const std::vector<msdf_atlas::GlyphGeometry>& GetGlyphs() const { return m_Glyphs; }
		const msdf_atlas::FontGeometry& GetGeometry() const { return m_FontGeometry; }
		inline std::string GetFontName() const { return m_FontName; }
		inline const DescriptorSet* GetUniform() const { return &m_DescriptorSet; }

	private:
		std::string m_FontName;
		std::vector<msdf_atlas::GlyphGeometry> m_Glyphs;
		msdf_atlas::FontGeometry m_FontGeometry;
		DescriptorSet m_DescriptorSet;
		Sampler m_Sampler;

		Ref<Image> m_AtlasTexture;
	};
}