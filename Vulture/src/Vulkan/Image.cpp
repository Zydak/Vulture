#include "pch.h"
#include "Utility/Utility.h"

#include "Image.h"
#include "Vulture/Renderer/Renderer.h"

#include <vulkan/vulkan_core.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Vulture/Math/Defines.h"

namespace Vulture
{
	void Image::Init(const CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		VL_CORE_ASSERT(createInfo, "Incorectly initialized image create info! Values");
		m_Usage = createInfo.Usage;
		m_MemoryProperties = createInfo.Properties;
		m_Allocation = new VmaAllocation();
		m_Size.width = createInfo.Width;
		m_Size.height = createInfo.Height;

		m_Format = createInfo.Format;
		m_Aspect = createInfo.Aspect;

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

		if (createInfo.DebugName != "")
		{
			Device::SetObjectName(VK_OBJECT_TYPE_IMAGE, (uint64_t)m_ImageHandle, createInfo.DebugName);
		}

		m_Initialized = true;
	}

	void Image::Init(const std::string& filepath, SamplerInfo samplerInfo /*= { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST }*/)
	{
		if (m_Initialized)
			Destroy();

		m_Format = VK_FORMAT_R8G8B8A8_UNORM;
		m_Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		bool HDR = filepath.find(".hdr") != std::string::npos;
		if (HDR)
		{
			m_Format = VK_FORMAT_R32G32B32A32_SFLOAT;
			// TODO: ability to choose
			CreateHDRImage(filepath, EnvMethod::ImportanceSampling, samplerInfo);
			return;
		}

		m_Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		m_MemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		m_Allocation = new VmaAllocation();
		int texChannels;
		stbi_set_flip_vertically_on_load(true);
		int sizeX = (int)m_Size.width, sizeY = (int)m_Size.height;
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
		m_Size.width = (uint32_t)sizeX;
		m_Size.height = (uint32_t)sizeY;
		//m_MipLevels = uint32_t(floor(log2(std::max(m_Size.Width, m_Size.Height)))) + 1;
		uint64_t sizeOfPixel = HDR ? sizeof(float) * 4 : sizeof(uint8_t) * 4;
		VkDeviceSize imageSize = (uint64_t)m_Size.width * (uint64_t)m_Size.height * sizeOfPixel;

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

		info.Height = (uint32_t)m_Size.height;
		info.Width = (uint32_t)m_Size.width;
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

		CopyBufferToImage(buffer.GetBuffer(), (uint32_t)m_Size.width, (uint32_t)m_Size.height);

		TransitionImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			0
		);

		CreateImageView(info.Format, VK_IMAGE_ASPECT_COLOR_BIT);

		Device::SetObjectName(VK_OBJECT_TYPE_IMAGE, (uint64_t)m_ImageHandle, filepath.c_str());

		m_Initialized = true;
	}

	void Image::Init(const glm::vec4& color, const CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		VL_CORE_ASSERT(createInfo, "Incorectly initialized image create info! Values");
		m_Usage = createInfo.Usage;
		m_MemoryProperties = createInfo.Properties;
		m_Allocation = new VmaAllocation();
		m_Size.width = (uint32_t)createInfo.Width;
		m_Size.height = (uint32_t)createInfo.Height;

		m_Format = createInfo.Format;
		m_Aspect = createInfo.Aspect;

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

		TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		CopyBufferToImage(buffer.GetBuffer(), (uint32_t)m_Size.width, (uint32_t)m_Size.height);
		TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		if (createInfo.DebugName != "")
		{
			Device::SetObjectName(VK_OBJECT_TYPE_IMAGE, (uint64_t)m_ImageHandle, createInfo.DebugName);
		}

		m_Initialized = true;
	}

	void Image::Destroy()
	{
		m_JointPDF.reset();
		m_CDFInverseX.reset();
		m_CDFInverseY.reset();
		m_ImportanceSmplAccel.reset();
		vkDestroyImageView(Device::GetDevice(), m_ImageView, nullptr);
		vmaDestroyImage(Device::GetAllocator(), m_ImageHandle, *m_Allocation);
		delete m_Allocation;

		for (auto view : m_LayersView)
		{
			vkDestroyImageView(Device::GetDevice(), view, nullptr);
		}

		m_Initialized = false;
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
		if (m_Initialized)
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
			viewInfo.image = m_ImageHandle;
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
				viewInfo.image = m_ImageHandle;
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

		Device::CreateImage(imageCreateInfo, m_ImageHandle, *m_Allocation, createInfo.Properties);
	}

	void Image::CreateHDRImage(const std::string& filepath, EnvMethod method, SamplerInfo samplerInfo)
	{
		m_Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		m_MemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		m_Allocation = new VmaAllocation();
		int texChannels;
		stbi_set_flip_vertically_on_load(false);
		int sizeX = (int)m_Size.width, sizeY = (int)m_Size.height;
		float* pixels;

		pixels = stbi_loadf(filepath.c_str(), &sizeX, &sizeY, &texChannels, STBI_rgb_alpha);
		std::filesystem::path cwd = std::filesystem::current_path();
		VL_CORE_ASSERT(pixels, "failed to load texture image! Path: {0}, Current working directory: {1}", filepath, cwd.string());
		m_Size.width = (uint32_t)sizeX;
		m_Size.height = (uint32_t)sizeY;

		if (method == EnvMethod::ImportanceSampling)
		{
			float average, integral;
			std::vector<EnvAccel> envAccel;
			if (sizeX == 1 && sizeY == 1) // dummy file loaded
			{
				envAccel.push_back({0, 0});
			}
			else
			{
				envAccel = CreateEnvAccel(pixels, sizeX, sizeY, average, integral);
			}

			Buffer::CreateInfo bufferInfo{};
			bufferInfo.InstanceCount = 1;
			bufferInfo.InstanceSize = sizeof(EnvAccel) * envAccel.size();
			bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			bufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

			Buffer stagingBuf(bufferInfo);
			stagingBuf.Map();
			stagingBuf.WriteToBuffer(envAccel.data());
			stagingBuf.Unmap();

			bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			bufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
			m_ImportanceSmplAccel = std::make_shared<Buffer>();
			m_ImportanceSmplAccel->Init(bufferInfo);

			Buffer::CopyBuffer(stagingBuf.GetBuffer(), m_ImportanceSmplAccel->GetBuffer(), stagingBuf.GetBufferSize(), 0, 0, Device::GetGraphicsQueue(), 0, Device::GetGraphicsCommandPool());
		}

		//m_MipLevels = uint32_t(floor(log2(std::max(m_Size.Width, m_Size.Height)))) + 1;
		uint64_t sizeOfPixel = sizeof(float) * 4;
		VkDeviceSize imageSize = (uint64_t)m_Size.width * (uint64_t)m_Size.height * sizeOfPixel;

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

		info.Format = VK_FORMAT_R32G32B32A32_SFLOAT;

		info.Height = (uint32_t)m_Size.height;
		info.Width = (uint32_t)m_Size.width;
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

		CopyBufferToImage(buffer.GetBuffer(), (uint32_t)m_Size.width, (uint32_t)m_Size.height);

		TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		CreateImageView(info.Format, VK_IMAGE_ASPECT_COLOR_BIT);

		if (method == EnvMethod::InverseTransformSampling)
		{
			m_JointPDF = std::make_shared<Image>();
			m_CDFInverseX = std::make_shared<Image>();
			m_CDFInverseY = std::make_shared<Image>();
			Renderer::SampleEnvMap(this);
		}

		m_Initialized = true;
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
		barrier.image = m_ImageHandle;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth =  (int32_t)m_Size.width;
		int32_t mipHeight = (int32_t)m_Size.height;

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
				m_ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				m_ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
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
	void Image::TransitionImageLayout(const VkImageLayout& newLayout, VkCommandBuffer cmdBuffer, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkImageSubresourceRange subresourceRange)
	{
		if (m_Type == ImageType::Cubemap)
			subresourceRange.layerCount = 6;

		VkCommandBuffer commandBuffer;

		if (!cmdBuffer)
			Device::BeginSingleTimeCommands(commandBuffer, Device::GetGraphicsCommandPool());
		else
			commandBuffer = cmdBuffer;

		switch (m_Layout)
		{
		case VK_IMAGE_LAYOUT_GENERAL:
			srcAccess |= VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			srcStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			srcAccess |= VK_ACCESS_SHADER_READ_BIT;
			srcStage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			srcAccess |= VK_ACCESS_TRANSFER_READ_BIT;
			srcStage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			srcAccess |= VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		default:
			break;
		}
		
		switch (newLayout)
		{
		case VK_IMAGE_LAYOUT_GENERAL:
			dstAccess |= VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
			dstStage |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			break;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			dstAccess |= VK_ACCESS_SHADER_READ_BIT;
			dstStage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dstAccess |= VK_ACCESS_TRANSFER_READ_BIT;
			dstStage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dstAccess |= VK_ACCESS_TRANSFER_WRITE_BIT;
			dstStage |= VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		default:
			break;
		}

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = m_Layout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_EXTERNAL;
		barrier.image = m_ImageHandle;

		if (m_Aspect == VK_IMAGE_ASPECT_COLOR_BIT)
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		else if (m_Aspect == VK_IMAGE_ASPECT_DEPTH_BIT)
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		barrier.subresourceRange = subresourceRange;
		barrier.srcAccessMask = srcAccess;
		barrier.dstAccessMask = dstAccess;
		VkPipelineStageFlags srcStageMask = srcStage;
		VkPipelineStageFlags dstStageMask = dstStage;

		vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		if (!cmdBuffer)
			Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());

		m_Layout = newLayout;
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

		vkCmdCopyBufferToImage(commandBuffer, buffer, m_ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());
	}

	/*
	 * @brief Copies data from one image to another using Vulkan commands.
	 *
	 * @param image - The source image containing the data to copy.
	 * @param width - The width of the image region to copy.
	 * @param height - The height of the image region to copy.
	 * @param layout - The layout of the destination image.
	 * @param srcOffset - The offset in the source image to copy the data from.
	 * @param dstOffset - The offset in the destination image to copy the data to.
	 */
	void Image::CopyImageToImage(VkImage image, uint32_t width, uint32_t height, VkImageLayout layout, VkCommandBuffer cmd = 0, VkOffset3D srcOffset, VkOffset3D dstOffset)
	{
		VkCommandBuffer commandBuffer;

		if (cmd == 0)
			Device::BeginSingleTimeCommands(commandBuffer, Device::GetGraphicsCommandPool());
		else
			commandBuffer = cmd;


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

		vkCmdCopyImage(commandBuffer, image, layout, m_ImageHandle, m_Layout, 1, &region);

		if (cmd == 0)
			Device::EndSingleTimeCommands(commandBuffer, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());
	}

	// TODO description
	float Image::GetLuminance(glm::vec3 color)
	{
		return color.r * 0.2126F + color.g * 0.7152F + color.b * 0.0722F;
	}

	// Create acceleration data for importance sampling
	// And store the PDF into the ALPHA channel of pixels
	std::vector<Image::EnvAccel> Image::CreateEnvAccel(float*& pixels, uint32_t width, uint32_t height, float& average, float& integral)
	{
		VL_CORE_INFO("Creating Env Accel Structure...");
		const uint32_t rx = width;
		const uint32_t ry = height;

		// Create importance sampling data
		std::vector<Image::EnvAccel> envAccel(rx * ry);
		std::vector<float> importanceData(rx * ry);

		float cosTheta0			= 1.0F; // cosine of the up vector
		const float stepPhi		= (float)2.0F * (float)M_PI / (float)rx; // azimuth step
		const float stepTheta	= (float)M_PI / (float)ry; // elevation step
		double total			= 0.0;

		// For each texel of the environment map, we compute the related solid angle
		// subtended by the texel, and store the weighted luminance in importance_data,
		// representing the amount of energy emitted through each texel.
		// Also compute the average CIE luminance to drive the tonemapping of the final image
		for (uint32_t y = 0; y < ry; ++y)
		{
			const float theta1 = (float)(y + 1) * stepTheta; // elevation angle of currently sampled texel
			const float cosTheta1 = glm::cos(theta1); // cosine of the elevation angle

			// Calculate how much area does each texel take
			// (cosTheta0 - cosTheta1) - how much of the unit sphere does texel take
			//  * stepPhi - get solid angle
			const float area = (cosTheta0 - cosTheta1) * stepPhi;  // solid angle
			cosTheta0 = cosTheta1; // set cosine of the up vector to the elevation cosine to advance the loop

			for (uint32_t x = 0; x < rx; ++x)
			{
				const uint32_t idx = y * rx + x;
				const uint32_t idx4 = idx * 4; // texel index
				float          luminance = GetLuminance(*(glm::vec3*)&pixels[idx4]);

				// Store the radiance of the texel into importance array, importance will be higher for brither texels
				importanceData[idx] = area * glm::max(pixels[idx4], glm::max(pixels[idx4 + 1], pixels[idx4 + 2]));
				total += luminance;
			}
		}

		// maybe I'll use this for tonemapping? idk
		average = float(total) / float(rx * ry);

		// Alias map is used to efficiently sslect texels from env map based on importance.
		// It aims at creating a set of texel couples
		// so that all couples emit roughly the same amount of energy. To do this,
		// each smaller radiance texel will be assigned an "alias" with higher emitted radiance
		integral = BuildAliasMap(importanceData, envAccel);

		// We deduce the PDF of each texel by normalizing its emitted radiance by the radiance integral
		for (uint32_t i = 0; i < rx * ry; ++i)
		{
			const uint32_t idx4 = i * 4;
			// Store the PDF inside Alpha channel(idx4 + 3)
			pixels[idx4 + 3] = glm::max(pixels[idx4], glm::max(pixels[idx4 + 1], pixels[idx4 + 2])) / integral;
		}

		return envAccel;
	}

	// Alias map is used to efficiently sslect texels from env map based on importance.
	// It aims at creating a set of texel couples
	// so that all couples emit roughly the same amount of energy. To do this,
	// each smaller radiance texel will be assigned an "alias" with higher emitted radiance
	float Image::BuildAliasMap(const std::vector<float>& data, std::vector<EnvAccel>& accel)
	{
		uint32_t size = uint32_t(data.size());

		// Compute the integral(sum) of the emitted radiance of the environment map
		// Since each element in data is already weighted by its solid angle
		// the integral is a simple sum
		float sum = std::accumulate(data.begin(), data.end(), 0.F);

		float average = sum / float(size);
		for (uint32_t i = 0; i < size; i++)
		{
			// Calculate PDF. Inside PDF average of all values must be equal to 1, that's
			// why we divide texel importance from data by the average of all texels
			accel[i].Importance = data[i] / average;

			// identity, ie. each texel is its own alias
			accel[i].Alias = i;
		}

		// Partition the texels according to their importance.
		// Texels with a value q < 1 (ie. below average) are stored incrementally from the beginning of the
		// array, while texels emitting higher-than-average radiance are stored from the end of the array.
		// This effectively separates the texels into two groups: one containing texels with below-average 
		// radiance and the other containing texels with above-average radiance
		std::vector<uint32_t> partitionTable(size);
		uint32_t              lowEnergyCounter = 0U;
		uint32_t              HighEnergyCounter = size;
		for (uint32_t i = 0; i < size; ++i)
		{
			if (accel[i].Importance < 1.F)
			{
				lowEnergyCounter++;
				partitionTable[lowEnergyCounter] = i;
			}
			else
			{
				HighEnergyCounter--;
				partitionTable[HighEnergyCounter] = i;
			}
		}

		// Associate the lower-energy texels to higher-energy ones. Since the emission of a high-energy texel may
		// be vastly superior to the average,
		for (lowEnergyCounter = 0; lowEnergyCounter < HighEnergyCounter && HighEnergyCounter < size; lowEnergyCounter++)
		{
			// Index of the smaller energy texel
			const uint32_t smallEnergyIndex = partitionTable[lowEnergyCounter];

			// Index of the higher energy texel
			const uint32_t highEnergyIndex = partitionTable[HighEnergyCounter];

			// Associate the texel to its higher-energy alias
			accel[smallEnergyIndex].Alias = highEnergyIndex;

			// Compute the difference between the lower-energy texel and the average
			const float differenceWithAverage = 1.F - accel[smallEnergyIndex].Importance;

			// The goal is to obtain texel couples whose combined intensity is close to the average.
			// However, some texels may have low intensity, while others may have very high intensity. In this case
			// it may not be possible to obtain a value close to average by combining only two texels.
			// Instead, we potentially associate a single high-energy texel to many smaller-energy ones until
			// the combined average is similar to the average of the environment map.
			// We keep track of the combined average by subtracting the difference between the lower-energy texel
			// importance and the average from the importance stored in the high-energy texel.

			// we can subtract directly from the accel[idx].Importance because in the shader we
			// compare it to values from 0-1 so it doesn't matter whether it's 1 or 1 million.
			// So until we stop subtracting at 1.0f we should be fine.
			accel[highEnergyIndex].Importance -= differenceWithAverage;

			// If the combined ratio to average of the higher-energy texel reaches 1, a balance has been found
			// between a set of low-energy texels and the higher-energy one. In this case, we will use the next
			// higher-energy texel in the partition when processing the next texel.
			if (accel[highEnergyIndex].Importance < 1.0f)
			{
				HighEnergyCounter++;
			}
		}
		// Return the integral of the emitted radiance. This integral will be used to normalize the probability
		// distribution function (PDF) of each pixel
		return sum;
	}

}