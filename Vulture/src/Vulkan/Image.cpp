#include "pch.h"
#include "Utility/Utility.h"

#include "Image.h"

#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Vulture
{

	Image::Image(const ImageInfo& imageInfo)
	{
		m_Size.Width = imageInfo.width;
		m_Size.Height = imageInfo.height;
		CreateImage(imageInfo.width, imageInfo.height, imageInfo.format, imageInfo.tiling, imageInfo.usage, 1, imageInfo.layerCount, imageInfo.type);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(Device::GetDevice(), m_Image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = Device::FindMemoryType(memRequirements.memoryTypeBits, imageInfo.properties);

		VL_CORE_RETURN_ASSERT(vkAllocateMemory(Device::GetDevice(), &allocInfo, nullptr, &m_ImageMemory),
			VK_SUCCESS, 
			"failed to allocate image memory!"
		);

		VL_CORE_RETURN_ASSERT(vkBindImageMemory(Device::GetDevice(), m_Image, m_ImageMemory, 0),
			VK_SUCCESS,
			"failed to bind image memory!"
		);

		if (imageInfo.type == ImageType::Cubemap)
		{
			CreateImageView(imageInfo.format, imageInfo.aspect, imageInfo.layerCount, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);
		}
		else if (imageInfo.layerCount > 1 || imageInfo.type == ImageType::Image2DArray)
		{
			CreateImageView(imageInfo.format, imageInfo.aspect, imageInfo.layerCount, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
		}
		else
		{
			CreateImageView(imageInfo.format, imageInfo.aspect, imageInfo.layerCount, VK_IMAGE_VIEW_TYPE_2D);
		}
		CreateImageSampler(imageInfo.samplerInfo);
	}

	Image::~Image()
	{
		vkDeviceWaitIdle(Device::GetDevice());
		vkDestroyImage(Device::GetDevice(), m_Image, nullptr);
		vkDestroyImageView(Device::GetDevice(), m_ImageView, nullptr);
		vkFreeMemory(Device::GetDevice(), m_ImageMemory, nullptr);

		for (auto view : m_LayersView)
		{
			vkDestroyImageView(Device::GetDevice(), view, nullptr);
		}
	}

	void Image::CreateImageView(VkFormat format, VkImageAspectFlagBits aspect, int layerCount, VkImageViewType imageType)
	{
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_Image;
			viewInfo.viewType = imageType;
			viewInfo.format = format;
			viewInfo.subresourceRange.aspectMask = aspect;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = m_MipLevels;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = layerCount;
			VL_CORE_RETURN_ASSERT(vkCreateImageView(Device::GetDevice(), &viewInfo, nullptr, &m_ImageView),
				VK_SUCCESS, 
				"failed to create image view!"
			);
		}

		if (layerCount > 1)
		{
			m_LayersView.resize(layerCount);
			for (int i = 0; i < layerCount; i++)
			{
				VkImageViewCreateInfo viewInfo{};
				viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				viewInfo.image = m_Image;
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewInfo.format = format;
				viewInfo.subresourceRange.aspectMask = aspect;
				viewInfo.subresourceRange.baseMipLevel = 0;
				viewInfo.subresourceRange.levelCount = m_MipLevels;
				viewInfo.subresourceRange.baseArrayLayer = i;
				viewInfo.subresourceRange.layerCount = 1;
				VL_CORE_RETURN_ASSERT(vkCreateImageView(Device::GetDevice(), &viewInfo, nullptr, &m_LayersView[i]),
					VK_SUCCESS,
					"failed to create image view layer!"
				);
			}
		}
	}

	void Image::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, uint32_t mipLevels, int layerCount, ImageType type)
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = layerCount;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (type == ImageType::Cubemap)
			imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		VL_CORE_RETURN_ASSERT(vkCreateImage(Device::GetDevice(), &imageInfo, nullptr, &m_Image),
			VK_SUCCESS,
			"failed to create image!"
		);
	}

	Image::Image(const std::string& filepath, SamplerInfo samplerInfo)
	{
		int texChannels;
		stbi_set_flip_vertically_on_load(true);
		stbi_uc* pixels = stbi_load(filepath.c_str(), &m_Size.Width, &m_Size.Height, &texChannels, STBI_rgb_alpha);
		m_MipLevels = static_cast<uint32_t>(floor(log2(std::max(m_Size.Width, m_Size.Height)))) + 1;
		VkDeviceSize imageSize = m_Size.Width * m_Size.Height * 4;

		VL_CORE_ASSERT(pixels, "failed to load texture image! " + filepath);

		auto buffer = std::make_unique<Buffer>(imageSize, 1, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		buffer->Map(imageSize);
		buffer->WriteToBuffer((void*)pixels, static_cast<uint32_t>(imageSize));
		buffer->Unmap();

		stbi_image_free(pixels);

		CreateImage(m_Size.Width, m_Size.Height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, m_MipLevels);

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(Device::GetDevice(), m_Image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = Device::FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		VL_CORE_RETURN_ASSERT(vkAllocateMemory(Device::GetDevice(), &allocInfo, nullptr, &m_ImageMemory),
			VK_SUCCESS,
			"failed to allocate image memory!"
		);

		VL_CORE_RETURN_ASSERT(vkBindImageMemory(Device::GetDevice(), m_Image, m_ImageMemory, 0),
			VK_SUCCESS,
			"failed to bind image memory!"
		);

		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = m_MipLevels;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		Image::TransitionImageLayout(m_Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, range);
		CopyBufferToImage(buffer->GetBuffer(), static_cast<uint32_t>(m_Size.Width), static_cast<uint32_t>(m_Size.Height));

		Image::TransitionImageLayout(m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		m_ImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		CreateImageView(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
		CreateImageSampler(samplerInfo);
	}

	void Image::CreateImageSampler(SamplerInfo samplerInfo)
	{
		m_Sampler = std::make_shared<Sampler>(samplerInfo, m_MipLevels > 1 ? m_MipLevels : 0);
	}

	void Image::GenerateMipmaps()
	{
		VkCommandBuffer commandBuffer;
		Device::BeginSingleTimeCommands(commandBuffer, Device::GetCommandPool());

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = m_Image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = m_Size.Width;
		int32_t mipHeight = m_Size.Height;

		for (uint32_t i = 1; i < m_MipLevels; i++)
		{
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(
				commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR
			);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = m_MipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetCommandPool());
	}

	void Image::TransitionImageLayout(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout, VkCommandBuffer cmdBuffer, const VkImageSubresourceRange& subresourceRange) {
		VkCommandBuffer commandBuffer;

		if (!cmdBuffer)
			Device::BeginSingleTimeCommands(commandBuffer, Device::GetCommandPool());
		else
			commandBuffer = cmdBuffer;

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange = subresourceRange;
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		// Source access mask controls actions that have to be finished on the old layout
		// before it will be transitioned to the new layout
		switch (oldLayout) {
		case VK_IMAGE_LAYOUT_UNDEFINED:
			// Image layout is undefined (or does not matter)
			// Only valid as initial layout
			// No flags required, listed only for completeness
			barrier.srcAccessMask = 0;
			break;

		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			// Image is preinitialized
			// Only valid as initial layout for linear images, preserves memory contents
			// Make sure host writes have been finished
			barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			// Image is a color attachment
			// Make sure any writes to the color buffer have been finished
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			// Image is a depth/stencil attachment
			// Make sure any writes to the depth/stencil buffer have been finished
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			// Image is a transfer source
			// Make sure any reads from the image have been finished
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			// Image is a transfer destination
			// Make sure any writes to the image have been finished
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			// Image is read by a shader
			// Make sure any shader reads from the image have been finished
			barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			// Other source layouts aren't handled (yet)
			break;
		}

		// Destination access mask controls the dependency for the new image layout
		switch (newLayout) {
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			// Image will be used as a transfer destination
			// Make sure any writes to the image have been finished
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			// Image will be used as a transfer source
			// Make sure any reads from the image have been finished
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			// Image will be used as a color attachment
			// Make sure any writes to the color buffer have been finished
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			// Image layout will be used as a depth/stencil attachment
			// Make sure any writes to depth/stencil buffer have been finished
			barrier.dstAccessMask = barrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			// Image will be read in a shader (sampler, input attachment)
			// Make sure any writes to the image have been finished
			if (barrier.srcAccessMask == 0) { barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT; }
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			// Other source layouts aren't handled (yet)
			break;
		}

		vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		if (!cmdBuffer)
			Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetCommandPool());
	}

	void Image::CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height, VkOffset3D offset) 
	{
		VkCommandBuffer commandBuffer;
		Device::BeginSingleTimeCommands(commandBuffer, Device::GetCommandPool());

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = offset;
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(commandBuffer, buffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetCommandPool());
	}

	void Image::CopyImageToImage(VkImage image, uint32_t width, uint32_t height, VkOffset3D offset)
	{
		VkCommandBuffer commandBuffer;
		Device::BeginSingleTimeCommands(commandBuffer, Device::GetCommandPool());

		VkImageCopy region{};

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;

		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.dstSubresource.mipLevel = 0;
		region.dstSubresource.baseArrayLayer = 0;
		region.dstSubresource.layerCount = 1;

		region.srcOffset = { 0, 0, 0 };
		region.dstOffset = offset;
		region.extent = { width, height, 1 };

		vkCmdCopyImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Image, m_ImageLayout, 1, &region);
		

		Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetCommandPool());
	}

}