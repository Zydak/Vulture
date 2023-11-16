#include "pch.h"
#include "TextureAtlas.h"

namespace Vulture
{

	TextureAtlas::TextureAtlas()
	{

	}

	TextureAtlas::~TextureAtlas()
	{

	}

	void TextureAtlas::AddTexture(const std::string& filepath)
	{
		auto it = std::find_if(m_Images.begin(), m_Images.end(), [filepath](const std::pair<std::string, Scope<Image>>& p) {
			return p.first == filepath;
			});

		VL_CORE_ASSERT(it == m_Images.end(), "Element already added");

		m_Images.push_back(std::make_pair(filepath, std::make_unique<Image>(filepath)));
	}

	void TextureAtlas::SetTiling(int tiling)
	{
		m_TilingSize = tiling;
	}

	void TextureAtlas::SetAtlasSize(glm::vec2 atlasSize)
	{
		m_AtlasSize = atlasSize;
	}

	static bool CompareImageSize(const std::pair<std::string, Scope<Image>>& image1, std::pair<std::string, Scope<Image>>& image2)
	{
		Size image1Size = image1.second->GetImageSize();
		Size image2Size = image2.second->GetImageSize();
		return image1Size.Width > image2Size.Width;//&& image1Size.Height > image2Size.Height;
	}

	void TextureAtlas::PackAtlas()
	{
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

		m_AtlasTexture = std::make_shared<Image>(info);

		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;
		Image::TransitionImageLayout(m_AtlasTexture->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, range);
		m_AtlasTexture->SetImageLayout(VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		std::sort(m_Images.begin(), m_Images.end(), CompareImageSize);
		
		for (auto& pair : m_Images)
		{
			std::string textureName = pair.first;
			auto& image = pair.second;
			
			Image::TransitionImageLayout(image->GetImage(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, range);
			
			if (m_LastOffset.x + (float)image->GetImageSize().Width >= m_AtlasSize.x)
			{
				m_LastOffset.x = 0;
				if (m_LastOffset.y + m_BiggestYInRow >= m_AtlasSize.y)
					VL_CORE_ASSERT(false, "Atlas is too small!");
				m_LastOffset.y += m_BiggestYInRow;
				m_BiggestYInRow = 0;
			}

			m_AtlasTexture->CopyImageToImage(
				image->GetImage(),
				image->GetImageSize().Width,
				image->GetImageSize().Height,
				{ (int)m_LastOffset.x, (int)m_LastOffset.y, 0 }
			);

			m_Textures[textureName] = m_LastOffset;
		
			if (m_BiggestYInRow < image->GetImageSize().Height)
			{
				m_BiggestYInRow += image->GetImageSize().Height;
			}
		
			m_LastOffset.x += (float)image->GetImageSize().Width;
			
		}

		Image::TransitionImageLayout(m_AtlasTexture->GetImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, range);
		m_AtlasTexture->SetImageLayout(VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

}