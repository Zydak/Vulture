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
		void Init(const std::string& filepath, const std::string& fontName, float fontSize, const glm::vec2& atlasSize);
		void Destroy();

		FontAtlas() = default;
		FontAtlas(const std::string& filepath, const std::string& fontName, float fontSize, const glm::vec2& atlasSize);
		~FontAtlas();

		FontAtlas(const FontAtlas& other) = delete;
		FontAtlas& operator=(const FontAtlas& other) = delete;
		FontAtlas(FontAtlas&& other) noexcept;
		FontAtlas& operator=(FontAtlas&& other) noexcept;

		Ref<Image> GetAtlasTexture() const { return m_AtlasTexture; }
		const std::vector<msdf_atlas::GlyphGeometry>& GetGlyphs() const { return m_Glyphs; }
		const msdf_atlas::FontGeometry& GetGeometry() const { return m_FontGeometry; }
		inline std::string GetFontName() const { return m_FontName; }
		inline DescriptorSet* GetUniform() { return &m_DescriptorSet; }

		inline bool IsInitialized() const { return m_Initialized; }

	private:
		std::string m_FontName = "";
		std::vector<msdf_atlas::GlyphGeometry> m_Glyphs;
		msdf_atlas::FontGeometry m_FontGeometry;
		DescriptorSet m_DescriptorSet;
		Sampler m_Sampler;

		Ref<Image> m_AtlasTexture = nullptr;

		bool m_Initialized = false;

		void Reset();
	};
}