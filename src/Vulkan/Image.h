#pragma once
#include "pch.h"

#include "Buffer.h"
#include "Device.h"
#include "Sampler.h"
#include "Vulture/Utility/Utility.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{
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
			void* Data = nullptr;
			const char* DebugName = "";

			VkFormat Format = VK_FORMAT_MAX_ENUM;
			VkImageUsageFlags Usage = 0;
			VkMemoryPropertyFlags Properties = 0;
			VkImageAspectFlagBits Aspect = VK_IMAGE_ASPECT_NONE;
			VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
			uint32_t Height = 0;
			uint32_t Width = 0;
			ImageType Type = ImageType::Image2D;
			int LayerCount = 1;
			int MipMapCount = 0;

			Vulture::SamplerInfo SamplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR };

			bool HDR = false;

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
		void Destroy();
		Image() = default;
		explicit Image(const CreateInfo& createInfo);

		explicit Image(const Image& other) = delete;
		Image& operator=(const Image& other) = delete;
		explicit Image(Image&& other) noexcept;
		Image& operator=(Image&& other) noexcept;

		~Image();
		void Resize(VkExtent2D newSize);

		void TransitionImageLayout(VkImageLayout newLayout, VkCommandBuffer cmdBuffer = 0, uint32_t baseLayer = 0);
		static void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkCommandBuffer cmdBuffer = 0, const VkImageSubresourceRange& subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		void CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height, uint32_t baseLayer = 0, VkCommandBuffer cmd = 0, VkOffset3D offset = {0, 0, 0});
		void CopyImageToImage(VkImage image, uint32_t width, uint32_t height, VkImageLayout layout, VkCommandBuffer cmd, VkOffset3D srcOffset = { 0, 0, 0 }, VkOffset3D dstOffset = {0, 0, 0});
		void BlitImageToImage(Image* srcImage, VkCommandBuffer cmd);

		void WritePixels(void* data, VkCommandBuffer cmd = 0, uint32_t baseLayer = 0);
		void GenerateMipmaps();
	public:

		inline VkImage GetImage() const { return m_ImageHandle; }
		inline VkImageView GetImageView(int layer = 0) const { return m_ImageViews[layer]; }
		inline std::vector<VkImageView> GetImageViews() const { return m_ImageViews; }
		inline VmaAllocationInfo GetAllocationInfo() const { VmaAllocationInfo info{}; vmaGetAllocationInfo(Device::GetAllocator(), *m_Allocation, &info); return info; }
		inline VkExtent2D GetImageSize() const { return m_Size; }
		inline VkImageUsageFlags GetUsageFlags() const { return m_Usage; }
		inline VkMemoryPropertyFlags GetMemoryProperties() const { return m_MemoryProperties; }
		inline VkImageLayout GetLayout() const { return m_Layout; }
		inline void SetLayout(VkImageLayout newLayout) { m_Layout = newLayout; }
		inline Buffer* GetAccelBuffer() { return &m_ImportanceSmplAccel; }
		inline uint32_t GetMipLevelsCount() const { return m_MipLevels; }
		inline bool IsInitialized() const { return m_Initialized; }
		inline VmaAllocation* GetAllocation() { return m_Allocation; }

	private:
		uint32_t FormatToSize(VkFormat format);
		void CreateImageView(VkFormat format, VkImageAspectFlagBits aspect, int layerCount = 1, VkImageViewType imageType = VK_IMAGE_VIEW_TYPE_2D);
		void CreateImage(const CreateInfo& createInfo);
		
		float GetLuminance(const glm::vec3& color);

		void CreateHDRSamplingBuffer(void* pixels);
		struct EnvAccel
		{
			uint32_t Alias;
			float Importance;
		}; 
		
		std::vector<EnvAccel> CreateEnvAccel(float* pixels, uint32_t width, uint32_t height, float& average, float& integral);
		float BuildAliasMap(const std::vector<float>& data, std::vector<EnvAccel>& accel);

		VkFormat m_Format = VK_FORMAT_MAX_ENUM;
		VkImageAspectFlagBits m_Aspect = VK_IMAGE_ASPECT_NONE;
		VkImageTiling m_Tiling = VK_IMAGE_TILING_OPTIMAL;
		int m_LayerCount = 1;
		ImageType m_Type = ImageType::Image2D;

		VkImage m_ImageHandle;
		std::vector<VkImageView> m_ImageViews; // view for each layer
		VmaAllocation* m_Allocation;
		VkExtent2D m_Size;
		uint32_t m_MipLevels = 1;

		bool m_Initialized = false;

		//Flags
		VkImageUsageFlags m_Usage;
		VkMemoryPropertyFlags m_MemoryProperties;
		VkImageLayout m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;

		// HDR only
		Buffer m_ImportanceSmplAccel;

		void Reset();
	};

}