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
		ColorRGBA32,
		ColorRG16,
		ColorRG32,
	};

	class Framebuffer
	{
	public:
		Framebuffer(const std::vector<FramebufferAttachment>& attachmentsFormats, VkRenderPass renderPass, VkExtent2D extent, VkFormat depthFormat, int layers = 1, ImageType type = ImageType::Image2D, VkImageUsageFlags customBits = 0);
		~Framebuffer();

		inline VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
		inline VkFramebuffer GetLayeredFramebuffer(int layer) const { if (!m_LayerFramebuffers.empty()) return m_LayerFramebuffers[layer]; else return GetFramebuffer(); }
		inline VkImageView GetColorImageView(int index) { return m_ColorAttachments[index].GetImageView(); }
		inline VkImageView GetColorImageViewLayered(int index, int layer) { return m_ColorAttachments[index].GetLayerView(layer); }
		inline VkImageView GetDepthImageViewLayered(int index, int layer) { return m_DepthAttachments[index].GetLayerView(layer); }
		inline VkImage GetColorImage(int index) { return m_ColorAttachments[index].GetImage(); }
		inline VkImageView GetDepthImageView(int index) { return m_DepthAttachments[index].GetImageView(); }
		inline VkImage GetDepthImage(int index) { return m_DepthAttachments[index].GetImage(); }

	private:
		void CreateColorAttachment(VkFormat format, int layers, ImageType type, VkImageUsageFlags customBits);
		void CreateDepthAttachment(int layers, ImageType type, VkImageUsageFlags customBits);

		VkExtent2D m_Extent;

		std::vector<VkImageView> m_Attachments;
		VkFramebuffer m_Framebuffer;
		std::vector<Image> m_ColorAttachments;
		std::vector<Image> m_DepthAttachments;

		std::vector<VkFramebuffer> m_LayerFramebuffers; // only for layered frame buffers

		VkFormat m_DepthFormat;
	};

}