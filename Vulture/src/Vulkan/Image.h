#pragma once
#include "pch.h"

#include "Buffer.h"
#include "Device.h"
#include "Sampler.h"
#include "Vulture/Utility/Utility.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{
	class AssetManager;

	// TODO Major rework, Add default constructors, change parameters to structs etc.
	class Image
	{
	public:
		enum class ImageType
		{
			Image2D,
			Image2DArray,
			Cubemap,
		};

		struct CreateInfo
		{
			uint32_t Width = 0;
			uint32_t Height = 0;
			VkFormat Format = VK_FORMAT_MAX_ENUM;
			VkImageUsageFlags Usage = 0;
			VkMemoryPropertyFlags Properties = 0;
			VkImageAspectFlagBits Aspect = VK_IMAGE_ASPECT_NONE;
			VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
			Vulture::SamplerInfo SamplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST };
			int LayerCount = 1;
			ImageType Type = ImageType::Image2D;

			operator bool() const
			{
				if (Width == 0 || Height == 0 || Format == VK_FORMAT_MAX_ENUM || Usage == 0 || Properties == 0 || Aspect == VK_IMAGE_ASPECT_NONE)
				{
					return false;
				}
				return true;
			}
		};

		void Init(const CreateInfo& createInfo);
		void Init(const std::string& filepath, SamplerInfo samplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST });
		void Init(const glm::vec4& color, const CreateInfo& createInfo);
		void Destroy();
		Image() = default;
		Image(const CreateInfo& createInfo);
		Image(const std::string& filepath, SamplerInfo samplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST });
		Image(const glm::vec4& color, const CreateInfo& createInfo);
		~Image();
		void TransitionImageLayout(const VkImageLayout& newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkCommandBuffer cmdBuffer = 0, const VkImageSubresourceRange& subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		static void TransitionImageLayout(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkCommandBuffer cmdBuffer = 0, const VkImageSubresourceRange& subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		void CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height, VkOffset3D offset = {0, 0, 0});
		void CopyImageToImage(VkImage image, uint32_t width, uint32_t height, VkImageLayout layout, VkOffset3D srcOffset = { 0, 0, 0 }, VkOffset3D dstOffset = {0, 0, 0});
		
	public:
		struct Info
		{
			VkFormat Format = VK_FORMAT_MAX_ENUM;
			VkImageAspectFlagBits Aspect = VK_IMAGE_ASPECT_NONE;
			VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
			int LayerCount = 1;
			ImageType Type = ImageType::Image2D;

			VkImage ImageHandle;
			VkImageView ImageView;
			std::vector<VkImageView> LayersView; // only for layered images
			VmaAllocation* Allocation;
			VkExtent2D Size;
			uint32_t MipLevels = 1;

			//Flags
			VkImageUsageFlags Usage;
			VkMemoryPropertyFlags MemoryProperties;
			VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;

			bool Initialized = false;
		};

		inline Info GetInfo() const { return m_Info; }

		inline VkImage GetImage() const { return m_Info.ImageHandle; }
		inline VkImageView GetImageView() const { return m_Info.ImageView; }
		inline VmaAllocationInfo GetAllocationInfo() const { VmaAllocationInfo info{}; vmaGetAllocationInfo(Device::GetAllocator(), *m_Info.Allocation, &info); return info; }
		inline VkSampler GetSampler() const { return m_Sampler->GetSampler(); }
		inline VkExtent2D GetImageSize() const { return m_Info.Size; }
		inline VkImageView GetLayerView(int layer) const { return m_Info.LayersView[layer]; }
		inline VkImageUsageFlags GetUsageFlags() const { return m_Info.Usage; }
		inline VkMemoryPropertyFlags GetMemoryProperties() const { return m_Info.MemoryProperties; }
		inline VkImageLayout GetLayout() const { return m_Info.Layout; }
		inline void SetLayout(VkImageLayout newLayout) { m_Info.Layout = newLayout; }

	private:
		void CreateImageView(VkFormat format, VkImageAspectFlagBits aspect, int layerCount = 1, VkImageViewType imageType = VK_IMAGE_VIEW_TYPE_2D);
		void CreateImage(const CreateInfo& createInfo);
		void GenerateMipmaps();
		void CreateImageSampler(SamplerInfo samplerInfo);

		Info m_Info;
		Ref<Sampler> m_Sampler;
	};

}