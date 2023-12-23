#pragma once

#include "pch.h"
#undef INFINITE

#include "msdf-atlas-gen/GlyphGeometry.h"
#include "msdf-atlas-gen/FontGeometry.h"

#include "Vulkan/Image.h"
#include "Vulkan/Uniform.h"


namespace Vulture
{
	class FontAtlas
	{
	public:
		FontAtlas(const std::string& filepath, const std::string& fontName);

		Ref<Image> GetAtlasTexture() { return m_AtlasTexture; }
		const std::vector<msdf_atlas::GlyphGeometry>& GetGlyphs() { return m_Glyphs; }
		const msdf_atlas::FontGeometry& GetGeometry() { return m_FontGeometry; }
		inline std::string GetFontName() const { return m_FontName; }
		inline Ref<Uniform> GetUniform() const { return m_Uniform; }

	private:
		std::string m_FontName;
		std::vector<msdf_atlas::GlyphGeometry> m_Glyphs;
		msdf_atlas::FontGeometry m_FontGeometry;
		Ref<Uniform> m_Uniform;
		Sampler m_Sampler;

		Ref<Image> m_AtlasTexture;
	};

}