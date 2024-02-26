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
			uint32_t Width = 0;
			uint32_t Height = 0;
			VkFormat Format = VK_FORMAT_MAX_ENUM;
			VkImageUsageFlags Usage = 0;
			VkMemoryPropertyFlags Properties = 0;
			VkImageAspectFlagBits Aspect = VK_IMAGE_ASPECT_NONE;
			VkImageTiling Tiling = VK_IMAGE_TILING_OPTIMAL;
			Vulture::SamplerInfo SamplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR };
			int LayerCount = 1;
			ImageType Type = ImageType::Image2D;

			const char* DebugName = "";

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
		void Init(const std::string& filepath, SamplerInfo samplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR });
		void Init(const glm::vec4& color, const CreateInfo& createInfo);
		void Destroy();
		Image() = default;
		Image(const CreateInfo& createInfo);
		Image(const std::string& filepath, SamplerInfo samplerInfo = { VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR });
		Image(const glm::vec4& color, const CreateInfo& createInfo);
		~Image();
		void TransitionImageLayout(const VkImageLayout& newLayout, VkCommandBuffer cmdBuffer = 0, VkAccessFlags srcAccess = 0, VkAccessFlags dstAccess = 0, VkPipelineStageFlags srcStage = 0, VkPipelineStageFlags dstStage = 0, VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		static void TransitionImageLayout(const VkImage& image, const VkImageLayout& oldLayout, const VkImageLayout& newLayout, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkCommandBuffer cmdBuffer = 0, const VkImageSubresourceRange& subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		void CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height, VkOffset3D offset = {0, 0, 0});
		void CopyImageToImage(VkImage image, uint32_t width, uint32_t height, VkImageLayout layout, VkCommandBuffer cmd, VkOffset3D srcOffset = { 0, 0, 0 }, VkOffset3D dstOffset = {0, 0, 0});
		
		Ref<Image> GetJointPDF() { return m_JointPDF; }
		Ref<Image> GetCDFInverseX() { return m_CDFInverseX; }
		Ref<Image> GetCDFInverseY() { return m_CDFInverseY; }
	public:

		inline VkImage GetImage() const { return m_ImageHandle; }
		inline VkImageView GetImageView() const { return m_ImageView; }
		inline VmaAllocationInfo GetAllocationInfo() const { VmaAllocationInfo info{}; vmaGetAllocationInfo(Device::GetAllocator(), *m_Allocation, &info); return info; }
		inline VkSampler GetSamplerHandle() const { return m_Sampler.GetSamplerHandle(); }
		inline VkExtent2D GetImageSize() const { return m_Size; }
		inline VkImageView GetLayerView(int layer) const { return m_LayersView[layer]; }
		inline VkImageUsageFlags GetUsageFlags() const { return m_Usage; }
		inline VkMemoryPropertyFlags GetMemoryProperties() const { return m_MemoryProperties; }
		inline VkImageLayout GetLayout() const { return m_Layout; }
		inline void SetLayout(VkImageLayout newLayout) { m_Layout = newLayout; }
		inline Ref<Buffer> GetAccelBuffer() { return m_ImportanceSmplAccel; }

	private:
		void CreateImageView(VkFormat format, VkImageAspectFlagBits aspect, int layerCount = 1, VkImageViewType imageType = VK_IMAGE_VIEW_TYPE_2D);
		void CreateImage(const CreateInfo& createInfo);
		void GenerateMipmaps();
		void CreateImageSampler(SamplerInfo samplerInfo);
		
		float GetLuminance(glm::vec3 color);

		enum class EnvMethod
		{
			InverseTransformSampling,
			ImportanceSampling
		};
		void CreateHDRImage(const std::string& filepath, EnvMethod method, SamplerInfo samplerInfo);
		struct EnvAccel
		{
			uint32_t Alias;
			float Importance;
		}; 
		
		std::vector<EnvAccel> CreateEnvAccel(float*& pixels, uint32_t width, uint32_t height, float& average, float& integral);
		float BuildAliasMap(const std::vector<float>& data, std::vector<EnvAccel>& accel);


		VkFormat m_Format = VK_FORMAT_MAX_ENUM;
		VkImageAspectFlagBits m_Aspect = VK_IMAGE_ASPECT_NONE;
		VkImageTiling m_Tiling = VK_IMAGE_TILING_OPTIMAL;
		int m_LayerCount = 1;
		ImageType m_Type = ImageType::Image2D;

		VkImage m_ImageHandle;
		VkImageView m_ImageView;
		std::vector<VkImageView> m_LayersView; // only for layered images
		VmaAllocation* m_Allocation;
		VkExtent2D m_Size;
		uint32_t m_MipLevels = 1;

		bool m_Initialized = false;
		Sampler m_Sampler;

		//Flags
		VkImageUsageFlags m_Usage;
		VkMemoryPropertyFlags m_MemoryProperties;
		VkImageLayout m_Layout = VK_IMAGE_LAYOUT_UNDEFINED;

		// HDR only
		Ref<Buffer> m_ImportanceSmplAccel;
		Ref<Image> m_JointPDF;
		Ref<Image> m_CDFInverseX;
		Ref<Image> m_CDFInverseY;
	};

}