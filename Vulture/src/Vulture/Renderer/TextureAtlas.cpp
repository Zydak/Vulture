#include "pch.h"
#include "TextureAtlas.h"
#include "Renderer/Renderer.h"

namespace Vulture
{

	TextureAtlas::TextureAtlas(const std::string& filepath)
	{
        m_AtlasTexture = std::make_shared<Image>(filepath);

		// Create a uniform for the texture atlas
		m_AtlasUniform = std::make_shared<Uniform>(Renderer::GetDescriptorPool());
		m_AtlasUniform->AddImageSampler(
			0,
			m_AtlasTexture->GetSampler(),
			m_AtlasTexture->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_SHADER_STAGE_FRAGMENT_BIT
		);
		m_AtlasUniform->AddUniformBuffer(
			1,
			sizeof(AtlasInfoBuffer),
			VK_SHADER_STAGE_FRAGMENT_BIT
		);
		m_AtlasUniform->Build();

		// Update the uniform buffer with atlas information
		AtlasInfoBuffer atlasInfo;
		atlasInfo.TilingSize = glm::vec4((float)m_TilingSize);
		m_AtlasUniform->GetBuffer(1)->WriteToBuffer(&atlasInfo, sizeof(AtlasInfoBuffer), 0);
		m_AtlasUniform->GetBuffer(1)->Flush();
	}

	TextureAtlas::~TextureAtlas()
	{

	}

    /*
	 * @brief Retrieves the texture offset for a given texture in the atlas.
	 *
	 * @param filepath The filepath of the texture.
	 * @return Reference to the texture offset in the atlas.
	 */
    glm::vec2 TextureAtlas::GetTextureOffset(const glm::vec2& tileOffset)
    {
		return tileOffset * glm::vec2((float)m_TilingSize);
    }

    /**
     * @brief Retrieves the uniform associated with the texture atlas.
     *
     * @return Pointer to the texture atlas uniform.
     */
    Ref<Uniform> TextureAtlas::GetAtlasUniform()
    {
        return m_AtlasUniform;
    }

    /*
     * @brief Sets the tiling size of the texture atlas.
     *
     * @param tiling - new tiling size.
     */
    void TextureAtlas::SetTiling(int tiling)
    {
        m_TilingSize = tiling;
    }

}