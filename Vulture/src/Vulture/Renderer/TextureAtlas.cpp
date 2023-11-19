#include "pch.h"
#include "TextureAtlas.h"
#include "Renderer/Renderer.h"

namespace Vulture
{

	TextureAtlas::TextureAtlas()
	{

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
    glm::vec2& TextureAtlas::GetTextureOffset(const std::string& filepath)
    {
        VL_CORE_ASSERT(m_Textures.find(filepath) != m_Textures.end(), "There is no such texture in atlas: {0}", filepath);
        return m_Textures[filepath];
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
     * @brief Adds a texture to the texture atlas.
     *
     * @param filepath - The filepath of the texture to be added.
     */
	void TextureAtlas::AddTexture(const std::string& filepath)
	{
		static int i = 0;

		// Check if the texture is already added
		auto it = std::find_if(m_Images.begin(), m_Images.end(), [filepath](const std::pair<std::string, Scope<Image>>& p) {
			return p.first == filepath;
			});

		VL_CORE_ASSERT(it == m_Images.end(), "Element already added");

		// Add the texture to the list
		m_Images.push_back(std::make_pair(filepath, std::make_unique<Image>(filepath)));

		// TODO: Textures that are bigger than tiling size are not supported currently
		VL_CORE_ASSERT(
			m_Images[i].second->GetImageSize().Width <= m_TilingSize || m_Images[i].second->GetImageSize().Height <= m_TilingSize,
			"Textures that are bigger than tiling size are not supported currently"
		);

		i++;
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

    /*
     * @brief Sets the size of the texture atlas.
     *
     * @param atlasSize - new size of the texture atlas.
     */
    void TextureAtlas::SetAtlasSize(glm::vec2 atlasSize)
    {
        m_AtlasSize = atlasSize;
    }

    /*
     * @brief Compares two images based on their sizes.
     *
     * @param image1 - The first image to compare.
     * @param image2 - The second image to compare.
     * @return True if the size of the first image is greater than the size of the second image, false otherwise.
     */
    static bool CompareImageSize(const std::pair<std::string, Scope<Image>>& image1, std::pair<std::string, Scope<Image>>& image2)
    {
        Size image1Size = image1.second->GetImageSize();
        Size image2Size = image2.second->GetImageSize();
        return image1Size.Width > image2Size.Width;
    }

    /*
	 * @brief Generates a texture atlas by arranging images based on their sizes.
	 * It also creates a uniform to be used in shaders with information about the atlas.
	 */
    void TextureAtlas::PackAtlas()
    {
        // TODO: way to automatically deduce atlas size
        ImageInfo info{};
        info.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.width = (uint32_t)m_AtlasSize.x;
        info.height = (uint32_t)m_AtlasSize.y;
        info.layerCount = 1;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.type = ImageType::Image2D;
        info.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.samplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST };

        // Create the texture atlas image
        m_AtlasTexture = std::make_shared<Image>(info);

        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        // Transition the image layout for copying
        Image::TransitionImageLayout(m_AtlasTexture->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, range);

        // Sort images based on size
        std::sort(m_Images.begin(), m_Images.end(), CompareImageSize);

        for (auto& pair : m_Images)
        {
            std::string textureName = pair.first;
            auto& image = pair.second;

            // Transition the individual image layout for copying
            Image::TransitionImageLayout(image->GetImage(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, range);

            // Check if a new row is needed in the atlas
            if (m_LastOffset.x + (float)image->GetImageSize().Width >= m_AtlasSize.x)
            {
                m_LastOffset.x = 0;
                if (m_LastOffset.y + m_BiggestYInRow >= m_AtlasSize.y)
                    VL_CORE_ASSERT(false, "Atlas is too small!");
                m_LastOffset.y += m_BiggestYInRow;
                m_BiggestYInRow = 0;
            }

            // Copy the image to the atlas
            m_AtlasTexture->CopyImageToImage(
                image->GetImage(),
                image->GetImageSize().Width,
                image->GetImageSize().Height,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                { 0, 0, 0 },
                { (int)m_LastOffset.x, (int)m_LastOffset.y, 0 }
            );

            // Update the texture coordinates for the image
            m_Textures[textureName] = m_LastOffset;

            // Update the biggest height in the current row
            if (m_BiggestYInRow < image->GetImageSize().Height)
            {
                m_BiggestYInRow += image->GetImageSize().Height;
            }

            // Update the offset for the next image
            m_LastOffset.x += (float)image->GetImageSize().Width;
        }

        // Transition the texture atlas image layout for shader access
        Image::TransitionImageLayout(m_AtlasTexture->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, range);

        // Create a uniform for the texture atlas
        m_AtlasUniform = std::make_shared<Uniform>(*Renderer::s_Pool);
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

}