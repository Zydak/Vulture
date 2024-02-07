#include "pch.h"
#include "TextureAtlas.h"
#include "Renderer/Renderer.h"

namespace Vulture
{

	void TextureAtlas::Init(const std::string& filepath)
	{
		if (m_Initialized)
			Destroy();

		m_AtlasTexture = std::make_shared<Image>(filepath);

		// Create a descriptor set for the texture atlas

		DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT };
		DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT };

		m_AtlasDescriptorSet = std::make_shared<DescriptorSet>();
		m_AtlasDescriptorSet->Init(&Renderer::GetDescriptorPool(), { bin, bin1 });
		m_AtlasDescriptorSet->AddImageSampler(
			0,
			m_AtlasTexture->GetSamplerHandle(),
			m_AtlasTexture->GetImageView(),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
		m_AtlasDescriptorSet->AddUniformBuffer(
			1,
			sizeof(AtlasInfoBuffer)
		);
		m_AtlasDescriptorSet->Build();

		// Update the uniform buffer with atlas information
		AtlasInfoBuffer atlasInfo;
		atlasInfo.TilingSize = glm::vec4((float)m_TilingSize);
		m_AtlasDescriptorSet->GetBuffer(1)->WriteToBuffer(&atlasInfo, sizeof(AtlasInfoBuffer), 0);
		m_AtlasDescriptorSet->GetBuffer(1)->Flush();

		m_Initialized = true;
	}

	void TextureAtlas::Destroy()
	{
		m_AtlasTexture.reset();
		m_AtlasDescriptorSet.reset();
		m_Initialized = false;
	}

	TextureAtlas::TextureAtlas(const std::string& filepath)
	{
		Init(filepath);
	}

	TextureAtlas::~TextureAtlas()
	{
		if (m_Initialized)
			Destroy();
	}

    /*
	 * @brief Retrieves the texture offset for a given texture in the atlas.
	 *
	 * @param filepath The filepath of the texture.
	 * @return Reference to the texture offset in the atlas.
	 */
    glm::vec2 TextureAtlas::GetTextureOffset(const glm::vec2& tileOffset) const
    {
		return tileOffset * glm::vec2((float)m_TilingSize);
    }

    /**
     * @brief Retrieves the descriptor set associated with the texture atlas.
     *
     * @return Pointer to the texture atlas descriptor set.
     */
    Ref<DescriptorSet> TextureAtlas::GetAtlasDescriptorSet() const
    {
        return m_AtlasDescriptorSet;
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