#pragma once
#include "pch.h"

#include "Device.h"
#include "Image.h"

#include <vulkan/vulkan.h>

namespace Vulture
{

	enum FramebufferAttachment
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
		Framebuffer(const std::vector<FramebufferAttachment>& attachmentsFormats, VkRenderPass renderPass, VkExtent2D extent, VkFormat depthFormat, int layers = 1, Image::ImageType type = Image::ImageType::Image2D, VkImageUsageFlags customBits = 0);
		~Framebuffer();

		inline VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
		inline VkFramebuffer GetLayeredFramebuffer(int layer) const { if (!m_LayerFramebuffers.empty()) return m_LayerFramebuffers[layer]; else return GetFramebuffer(); }
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
		VkFramebuffer m_Framebuffer;
		std::vector<Image> m_ColorAttachments;
		std::vector<Image> m_DepthAttachments;

		std::vector<VkFramebuffer> m_LayerFramebuffers; // only for layered frame buffers

		VkFormat m_DepthFormat;
	};

}