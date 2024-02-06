#pragma once
#include "pch.h"

#include "Device.h"
#include "Image.h"

#include <vulkan/vulkan.h>

namespace Vulture
{
	enum class FramebufferAttachment
	{
		Depth,
		ColorRGBA8,
		ColorRGBA16,
		ColorRGB8,
		ColorRGB16,
		ColorRGBA32,
		ColorRGBA32Float,
		ColorRG8,
		ColorRG16,
		ColorRG32,
		ColorR8,
	};

	class Framebuffer
	{
	public:
		struct CreateInfo
		{
			const std::vector<FramebufferAttachment>* AttachmentsFormats;
			VkRenderPass RenderPass = 0;
			VkExtent2D Extent = { 0, 0 };
			VkFormat DepthFormat = VK_FORMAT_MAX_ENUM;
			int Layers = 1;
			Image::ImageType Type = Image::ImageType::Image2D;
			VkImageUsageFlags CustomBits = 0;

			operator bool() const
			{
				return (!AttachmentsFormats->empty()) && (RenderPass != 0) && (Extent.width != 0 || Extent.height != 0) && (DepthFormat != VK_FORMAT_MAX_ENUM);
			}
		};

		void Init(const CreateInfo& createInfo);
		void Destroy();

		Framebuffer() = default;
		Framebuffer(const CreateInfo& createInfo);
		~Framebuffer();

		inline VkFramebuffer GetFramebufferHandle() const { return m_FramebufferHandle; }
		inline VkFramebuffer GetLayeredFramebuffer(int layer) const { if (!m_LayerFramebuffers.empty()) return m_LayerFramebuffers[layer]; else return GetFramebufferHandle(); }
		inline VkImageView GetColorImageView(int index) const { return m_ColorAttachments[index].GetImageView(); }
		inline VkImageView GetColorImageViewLayered(int index, int layer) const { return m_ColorAttachments[index].GetLayerView(layer); }
		inline VkImageView GetDepthImageViewLayered(int index, int layer) const { return m_DepthAttachments[index].GetLayerView(layer); }
		inline VkImage GetColorImage(int index) const { return m_ColorAttachments[index].GetImage(); }
		inline const Image* GetColorImageNoVk(int index) const { return &m_ColorAttachments[index]; }
		inline Image* GetColorImageNoVk(int index) { return &m_ColorAttachments[index]; }
		inline VkImageView GetDepthImageView(int index) const { return m_DepthAttachments[index].GetImageView(); }
		inline VkImage GetDepthImage(int index) const { return m_DepthAttachments[index].GetImage(); }

		inline uint32_t GetColorAttachmentCount() const { return (uint32_t)m_ColorAttachments.size(); }
		inline uint32_t GetDepthAttachmentCount() const { return (uint32_t)m_DepthAttachments.size(); }

		inline std::vector<FramebufferAttachment> GetDepthAttachmentFormats() const { return m_AttachmentFormats; }

		inline glm::vec2 GetExtent() const { return { m_Extent.width, m_Extent.height }; }

	private:
		void CreateColorAttachment(VkFormat format, int layers, Image::ImageType type, VkImageUsageFlags customBits);
		void CreateDepthAttachment(int layers, Image::ImageType type, VkImageUsageFlags customBits);

		VkExtent2D m_Extent;

		std::vector<VkImageView> m_Attachments;
		std::vector<FramebufferAttachment> m_AttachmentFormats;
		VkFramebuffer m_FramebufferHandle;
		std::vector<Image> m_ColorAttachments;
		std::vector<Image> m_DepthAttachments;

		std::vector<VkFramebuffer> m_LayerFramebuffers; // only for layered frame buffers

		VkFormat m_DepthFormat;

		bool m_Initialized = false;
	};

}