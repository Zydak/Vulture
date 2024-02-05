#include "pch.h"
#include "Utility/Utility.h"

#include "Image.h"

#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Vulture
{
	void Image::Init(const CreateInfo& createInfo)
	{
		VL_CORE_ASSERT(createInfo, "Incorectly initialized image create info! Values");
		m_Info.Usage = createInfo.Usage;
		m_Info.MemoryProperties = createInfo.Properties;
		m_Info.Allocation = new VmaAllocation();
		m_Info.Size.width = createInfo.Width;
		m_Info.Size.height = createInfo.Height;
		CreateImage(createInfo);

		if (createInfo.Type == ImageType::Cubemap)
		{
			CreateImageView(createInfo.Format, createInfo.Aspect, createInfo.LayerCount, VK_IMAGE_VIEW_TYPE_CUBE);
		}
		else if (createInfo.LayerCount > 1 || createInfo.Type == ImageType::Image2DArray)
		{
			CreateImageView(createInfo.Format, createInfo.Aspect, createInfo.LayerCount, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
		}
		else
		{
			CreateImageView(createInfo.Format, createInfo.Aspect, createInfo.LayerCount, VK_IMAGE_VIEW_TYPE_2D);
		}
		CreateImageSampler(createInfo.SamplerInfo);
	}

	void Image::Init(const std::string& filepath, SamplerInfo samplerInfo /*= { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST }*/)
	{
		bool HDR = filepath.find(".hdr") != std::string::npos;

		m_Info.Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		m_Info.MemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		m_Info.Allocation = new VmaAllocation();
		int texChannels;
		stbi_set_flip_vertically_on_load(true);
		int sizeX = (int)m_Info.Size.width, sizeY = (int)m_Info.Size.height;
		void* pixels;
		if (HDR)
		{
			pixels = stbi_loadf(filepath.c_str(), &sizeX, &sizeY, &texChannels, STBI_rgb_alpha);

		}
		else
		{
			pixels = stbi_load(filepath.c_str(), &sizeX, &sizeY, &texChannels, STBI_rgb_alpha);
		}

		std::filesystem::path cwd = std::filesystem::current_path();
		VL_CORE_ASSERT(pixels, "failed to load texture image! Path: {0}, Current working directory: {1}", filepath, cwd.string());
		m_Info.Size.width = (uint32_t)sizeX;
		m_Info.Size.height = (uint32_t)sizeY;
		//m_MipLevels = static_cast<uint32_t>(floor(log2(std::max(m_Size.Width, m_Size.Height)))) + 1;
		uint64_t sizeOfPixel = HDR ? sizeof(float) * 4 : sizeof(uint8_t) * 4;
		VkDeviceSize imageSize = (uint64_t)m_Info.Size.width * (uint64_t)m_Info.Size.height * sizeOfPixel;

		Buffer buffer = Buffer();
		Buffer::CreateInfo BufferInfo{};
		BufferInfo.InstanceSize = imageSize;
		BufferInfo.InstanceCount = 1;
		BufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		BufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		buffer.Init(BufferInfo);

		auto res = buffer.Map(imageSize);
		buffer.WriteToBuffer((void*)pixels, (uint32_t)imageSize);
		buffer.Unmap();

		stbi_image_free(pixels);

		CreateInfo info;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;

		if (HDR)
		{
			info.Format = VK_FORMAT_R32G32B32A32_SFLOAT;
		}
		else
		{
			info.Format = VK_FORMAT_R8G8B8A8_UNORM;
		}

		info.Height = (uint32_t)m_Info.Size.height;
		info.Width = (uint32_t)m_Info.Size.width;
		info.LayerCount = 1;
		info.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		CreateImage(info);

		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		//range.levelCount = m_MipLevels;
		range.levelCount = 1;
		range.baseArrayLayer = 0;
		range.layerCount = 1;

		TransitionImageLayout(
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			0,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			range
		);

		CopyBufferToImage(buffer.GetBuffer(), (uint32_t)m_Info.Size.width, (uint32_t)m_Info.Size.height);

		TransitionImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		);

		CreateImageView(info.Format, VK_IMAGE_ASPECT_COLOR_BIT);
		CreateImageSampler(samplerInfo);
	}

	void Image::Init(const glm::vec4& color, const CreateInfo& createInfo)
	{
		VL_CORE_ASSERT(createInfo, "Incorectly initialized image create info! Values");
		m_Info.Usage = createInfo.Usage;
		m_Info.MemoryProperties = createInfo.Properties;
		m_Info.Allocation = new VmaAllocation();
		m_Info.Size.width = (uint32_t)createInfo.Width;
		m_Info.Size.height = (uint32_t)createInfo.Height;
		CreateImage(createInfo);

		if (createInfo.Type == ImageType::Cubemap)
		{
			CreateImageView(createInfo.Format, createInfo.Aspect, createInfo.LayerCount, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);
		}
		else if (createInfo.LayerCount > 1 || createInfo.Type == ImageType::Image2DArray)
		{
			CreateImageView(createInfo.Format, createInfo.Aspect, createInfo.LayerCount, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
		}
		else
		{
			CreateImageView(createInfo.Format, createInfo.Aspect, createInfo.LayerCount, VK_IMAGE_VIEW_TYPE_2D);
		}
		CreateImageSampler(createInfo.SamplerInfo);

		uint32_t size = createInfo.Width * createInfo.Height * sizeof(float);

		uint32_t* pixels = new uint32_t[size];
		for (int i = 0; i < (int)size; i++)
		{
			uint8_t r = (uint8_t)(color.r * 255.0f);
			uint8_t g = (uint8_t)(color.g * 255.0f);
			uint8_t b = (uint8_t)(color.b * 255.0f);
			uint8_t a = (uint8_t)(color.a * 255.0f);
			pixels[i] = (a << 24) | (b << 16) | (g << 8) | r;
		}
		Buffer::CreateInfo bufferCreateInfo{};
		bufferCreateInfo.InstanceSize = size;
		bufferCreateInfo.InstanceCount = 1;
		bufferCreateInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		Buffer buffer;
		buffer.Init(bufferCreateInfo);
		buffer.Map(size);
		buffer.WriteToBuffer((void*)pixels, size);
		buffer.Unmap();

		TransitionImageLayout(
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			0,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			0,
			VK_PIPELINE_STAGE_TRANSFER_BIT
		);
		CopyBufferToImage(buffer.GetBuffer(), (uint32_t)m_Info.Size.width, (uint32_t)m_Info.Size.height);
		TransitionImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
		);
	}

	void Image::Destroy()
	{
		//vkDeviceWaitIdle(Device::GetDevice());
		vkDestroyImageView(Device::GetDevice(), m_Info.ImageView, nullptr);
		vmaDestroyImage(Device::GetAllocator(), m_Info.ImageHandle, *m_Info.Allocation);
		delete m_Info.Allocation;

		for (auto view : m_Info.LayersView)
		{
			vkDestroyImageView(Device::GetDevice(), view, nullptr);
		}
	}

	/**
	 * @brief Initializes an Image object using the provided image information, creating the image, allocating memory,
	 * binding the memory, and creating the image view and sampler accordingly.
	 *
	 * @param imageInfo - The information required to create the image, including dimensions, format, tiling, usage, etc.
	 */
	Image::Image(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	Image::Image(const glm::vec4& color, const CreateInfo& createInfo)
	{
		Init(color, createInfo);
	}

	/*
	 * @brief Loads an image from the specified file, creates an image, allocates memory for it,
	 * binds the memory to the image, and sets up the image view and sampler.
	 *
	 * @param filepath - The file path of the image.
	 * @param samplerInfo - The sampler information for the image.
	 */
	Image::Image(const std::string& filepath, SamplerInfo samplerInfo)
	{
		Init(filepath, samplerInfo);
	}

	Image::~Image()
	{
		if (m_Info.Initialized)
		{
			Destroy();
		}
	}

	/*
	 * @brief Creates an image view for the image based on the provided format, aspect, layer count, and image type.
	 * It also handles the creation of individual layer views when the layer count is greater than 1.
	 *
	 * @param format - The format of the image view.
	 * @param aspect - The aspect of the image view.
	 * @param layerCount - The number of layers for the image view.
	 * @param imageType - The type of the image view.
	 */
	void Image::CreateImageView(VkFormat format, VkImageAspectFlagBits aspect, int layerCount, VkImageViewType imageType)
	{
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = m_Info.ImageHandle;
			viewInfo.viewType = imageType;
			viewInfo.format = format;
			viewInfo.subresourceRange.aspectMask = aspect;
			viewInfo.subresourceRange.baseMipLevel = 0;
			viewInfo.subresourceRange.levelCount = m_Info.MipLevels;
			viewInfo.subresourceRange.baseArrayLayer = 0;
			viewInfo.subresourceRange.layerCount = layerCount;
			VL_CORE_RETURN_ASSERT(vkCreateImageView(Device::GetDevice(), &viewInfo, nullptr, &m_Info.ImageView),
				VK_SUCCESS, 
				"failed to create image view!"
			);
		}

		if (layerCount > 1)
		{
			m_Info.LayersView.resize(layerCount);
			for (int i = 0; i < layerCount; i++)
			{
				VkImageViewCreateInfo viewInfo{};
				viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				viewInfo.image = m_Info.ImageHandle;
				viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewInfo.format = format;
				viewInfo.subresourceRange.aspectMask = aspect;
				viewInfo.subresourceRange.baseMipLevel = 0;
				viewInfo.subresourceRange.levelCount = m_Info.MipLevels;
				viewInfo.subresourceRange.baseArrayLayer = i;
				viewInfo.subresourceRange.layerCount = 1;
				VL_CORE_RETURN_ASSERT(vkCreateImageView(Device::GetDevice(), &viewInfo, nullptr, &m_Info.LayersView[i]),
					VK_SUCCESS,
					"failed to create image view layer!"
				);
			}
		}
	}

	/*
	 * @brief Creates an image with the specified parameters.
	 */
	void Image::CreateImage(const CreateInfo& createInfo)
	{
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = createInfo.Width;
		imageCreateInfo.extent.height = createInfo.Height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = createInfo.LayerCount;
		imageCreateInfo.format = createInfo.Format;
		imageCreateInfo.tiling = createInfo.Tiling;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = createInfo.Usage;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (createInfo.Type == ImageType::Cubemap)
			imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

		Device::CreateImage(imageCreateInfo, m_Info.ImageHandle, *m_Info.Allocation, createInfo.Properties);
	}

	/*
	 * @brief Creates a sampler for the image using the specified sampler information.
	 *
	 * @param samplerInfo - The sampler information for the image.
	 */
	void Image::CreateImageSampler(SamplerInfo samplerInfo)
	{
		m_Sampler = std::make_shared<Sampler>(samplerInfo);
	}

	/*
	 * @brief Generates mipmaps for the image.
	 */
	void Image::GenerateMipmaps()
	{
		VkCommandBuffer commandBuffer;
		Device::BeginSingleTimeCommands(commandBuffer, Device::GetGraphicsCommandPool());

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = m_Info.ImageHandle;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth =  (int32_t)m_Info.Size.width;
		int32_t mipHeight = (int32_t)m_Info.Size.height;

		for (uint32_t i = 1; i < m_Info.MipLevels; i++)
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
				m_Info.ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				m_Info.ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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

		barrier.subresourceRange.baseMipLevel = m_Info.MipLevels - 1;
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

		Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());
	}

	/*
	 * @brief Transitions the image layout from oldLayout to newLayout.
	 *
	 * @param image - The Vulkan image to transition.
	 * @param oldLayout - The old layout of the image.
	 * @param newLayout - The new layout to transition the image to.
	 * @param cmdBuffer - Optional command buffer for the transition (useful for custom command buffer recording).
	 * @param subresourceRange - Optional subresource range for the transition.
	 */
	void Image::TransitionImageLayout(const VkImageLayout& newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkCommandBuffer cmdBuffer, const VkImageSubresourceRange& subresourceRange)
	{
		VkCommandBuffer commandBuffer;

		if (!cmdBuffer)
			Device::BeginSingleTimeCommands(commandBuffer, Device::GetGraphicsCommandPool());
		else
			commandBuffer = cmdBuffer;

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = m_Info.Layout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = m_Info.ImageHandle;
		barrier.subresourceRange = subresourceRange;
		barrier.srcAccessMask = srcAccess;
		barrier.dstAccessMask = dstAccess;
		VkPipelineStageFlags srcStageMask = srcStage;
		VkPipelineStageFlags dstStageMask = dstStage;

		vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		if (!cmdBuffer)
			Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());

		m_Info.Layout = newLayout;
	}

	void Image::TransitionImageLayout(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkCommandBuffer cmdBuffer /*= 0*/, const VkImageSubresourceRange& subresourceRange /*= { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }*/)
	{
		VkCommandBuffer commandBuffer;

		if (!cmdBuffer)
			Device::BeginSingleTimeCommands(commandBuffer, Device::GetGraphicsCommandPool());
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

		barrier.dstAccessMask = dstAccess;
		barrier.srcAccessMask = srcAccess;

		vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		if (!cmdBuffer)
			Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());
	}

	/**
	 * @brief Copies data from a buffer to the image.
	 *
	 * @param buffer - The source buffer containing the data to copy.
	 * @param width - The width of the image region to copy.
	 * @param height - The height of the image region to copy.
	 * @param offset - The offset in the image to copy the data to.
	 */
	void Image::CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height, VkOffset3D offset) 
	{
		VkCommandBuffer commandBuffer;
		Device::BeginSingleTimeCommands(commandBuffer, Device::GetGraphicsCommandPool());

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

		vkCmdCopyBufferToImage(commandBuffer, buffer, m_Info.ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());
	}

	/*
	 * @briefCopies data from one image to another using Vulkan commands.
	 *
	 * @param image - The source image containing the data to copy.
	 * @param width - The width of the image region to copy.
	 * @param height - The height of the image region to copy.
	 * @param layout - The layout of the destination image.
	 * @param srcOffset - The offset in the source image to copy the data from.
	 * @param dstOffset - The offset in the destination image to copy the data to.
	 */
	void Image::CopyImageToImage(VkImage image, uint32_t width, uint32_t height, VkImageLayout layout, VkOffset3D srcOffset, VkOffset3D dstOffset)
	{
		VkCommandBuffer commandBuffer;
		Device::BeginSingleTimeCommands(commandBuffer, Device::GetGraphicsCommandPool());

		VkImageCopy region{};

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;

		region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.dstSubresource.mipLevel = 0;
		region.dstSubresource.baseArrayLayer = 0;
		region.dstSubresource.layerCount = 1;

		region.srcOffset = srcOffset;
		region.dstOffset = dstOffset;
		region.extent = { width, height, 1 };

		vkCmdCopyImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_Info.ImageHandle, layout, 1, &region);

		Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());
	}

}