#include "pch.h"
#include "Utility/Utility.h"

#include "Framebuffer.h"
#include <vulkan/vulkan_core.h>

namespace Vulture
{

	/*
	 * @brief Initializes a Framebuffer object with color and depth attachments based on the specified
	 * attachmentsFormats, renderPass, extent, depthFormat, layers, type, and customBits parameters. It creates
	 * the required attachments and framebuffers accordingly.
	 *
	 * @param attachmentsFormats - Vector specifying the format of each attachment in the framebuffer.
	 * @param renderPass - The Vulkan render pass to be used with the framebuffer.
	 * @param extent - The extent (width and height) of the framebuffer.
	 * @param depthFormat - The Vulkan format for the depth attachment.
	 * @param layers - The number of layers in the framebuffer.
	 * @param type - The type of image attachments.
	 * @param customBits - Additional custom Vulkan image usage flags.
	 */
	Framebuffer::Framebuffer(const std::vector<FramebufferAttachment>& attachmentsFormats,
		VkRenderPass renderPass, VkExtent2D extent, VkFormat depthFormat, int layers, ImageType type, VkImageUsageFlags customBits)
		: m_Extent(extent), m_DepthFormat(depthFormat)
	{
		m_ColorAttachments.reserve(attachmentsFormats.size());
		m_DepthAttachments.reserve(attachmentsFormats.size());
		for (int i = 0; i < attachmentsFormats.size(); i++) 
		{
			m_AttachmentFormats.push_back(attachmentsFormats[i]);
			switch (attachmentsFormats[i]) 
			{
			case FramebufferAttachment::ColorRGBA32: CreateColorAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, layers, type, customBits); break;
			case FramebufferAttachment::ColorRGBA16: CreateColorAttachment(VK_FORMAT_R16G16B16A16_UNORM, layers, type, customBits); break;
			case FramebufferAttachment::ColorRGBA8: CreateColorAttachment(VK_FORMAT_R8G8B8A8_UNORM, layers, type, customBits); break;

			case FramebufferAttachment::ColorRGB16: CreateColorAttachment(VK_FORMAT_R16G16B16_UNORM, layers, type, customBits); break;
			case FramebufferAttachment::ColorRGB8: CreateColorAttachment(VK_FORMAT_R8G8B8_UNORM, layers, type, customBits); break;

			case FramebufferAttachment::ColorRG16: CreateColorAttachment(VK_FORMAT_R16G16_UNORM, layers, type, customBits); break;
			case FramebufferAttachment::ColorRG32: CreateColorAttachment(VK_FORMAT_R32G32_SFLOAT, layers, type, customBits); break;

			case FramebufferAttachment::ColorR8: CreateColorAttachment(VK_FORMAT_R8_UNORM, layers, type, customBits); break;

			case FramebufferAttachment::Depth: CreateDepthAttachment(layers, type, customBits); break;
			}
		}

		for (int i = 0; i < m_ColorAttachments.size(); i++) m_Attachments.push_back(m_ColorAttachments[i].GetImageView());
		for (int i = 0; i < m_DepthAttachments.size(); i++) m_Attachments.push_back(m_DepthAttachments[i].GetImageView());

		{
			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = (uint32_t)m_Attachments.size();
			framebufferInfo.pAttachments = m_Attachments.data();
			framebufferInfo.width = m_Extent.width;
			framebufferInfo.height = m_Extent.height;
			framebufferInfo.layers = layers;

			VL_CORE_RETURN_ASSERT(vkCreateFramebuffer(Device::GetDevice(), &framebufferInfo, nullptr, &m_Framebuffer),
				VK_SUCCESS,
				"failed to create framebuffer!"
			);
		}

		if (layers > 1)
		{
			m_LayerFramebuffers.resize(layers);
			for (int i = 0; i < layers; i++)
			{
				m_Attachments.clear();
				for (int j = 0; j < m_ColorAttachments.size(); j++) m_Attachments.push_back(m_ColorAttachments[j].GetLayerView(i));
				for (int j = 0; j < m_DepthAttachments.size(); j++) m_Attachments.push_back(m_DepthAttachments[j].GetLayerView(i));
				VkFramebufferCreateInfo framebufferInfo = {};
				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = renderPass;
				framebufferInfo.attachmentCount = static_cast<uint32_t>(m_Attachments.size());
				framebufferInfo.pAttachments = m_Attachments.data();
				framebufferInfo.width = m_Extent.width;
				framebufferInfo.height = m_Extent.height;
				framebufferInfo.layers = 1;

				VL_CORE_RETURN_ASSERT(vkCreateFramebuffer(Device::GetDevice(), &framebufferInfo, nullptr, &m_LayerFramebuffers[i]),
					VK_SUCCESS,
					"failed to create framebuffer layer!"
				);
			}
		}
	}

	Framebuffer::~Framebuffer()
	{
		vkDestroyFramebuffer(Device::GetDevice(), m_Framebuffer, nullptr);
		for (auto framebuffer : m_LayerFramebuffers)
		{
			vkDestroyFramebuffer(Device::GetDevice(), framebuffer, nullptr);
		}
	}

	/**
	 * @brief Pushes a color attachment for the framebuffer using the specified format, layers, image type, and custom usage flags to m_ColorAttachments.
	 *
	 * @param format - The Vulkan format of the color attachment.
	 * @param layers - The number of layers in the color attachment.
	 * @param type - The image type of the color attachment.
	 * @param customBits - Additional custom Vulkan image usage flags.
	 */
	void Framebuffer::CreateColorAttachment(VkFormat format, int layers, ImageType type, VkImageUsageFlags customBits)
	{
		ImageInfo imageInfo;
		imageInfo.width = m_Extent.width;
		imageInfo.height = m_Extent.height;
		imageInfo.format = format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = customBits | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		imageInfo.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		imageInfo.samplerInfo = SamplerInfo(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR);
		imageInfo.layerCount = layers;
		imageInfo.type = type;

		m_ColorAttachments.emplace_back(imageInfo);
	}

	/**
	 * @brief Pushes a depth attachment for the framebuffer using the specified layers, image type, and custom usage flags to m_DepthAttachments.
	 *
	 * @param format - The Vulkan format of the color attachment.
	 * @param layers - The number of layers in the color attachment.
	 * @param type - The image type of the color attachment.
	 * @param customBits - Additional custom Vulkan image usage flags.
	 */
	void Framebuffer::CreateDepthAttachment(int layers, ImageType type, VkImageUsageFlags customBits)
	{
		customBits &= ~VK_IMAGE_USAGE_STORAGE_BIT; // Don't use storage bit on depth attachments
		ImageInfo imageInfo;
		imageInfo.width = m_Extent.width;
		imageInfo.height = m_Extent.height;
		imageInfo.format = m_DepthFormat;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = customBits | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		imageInfo.aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageInfo.samplerInfo = SamplerInfo(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR);
		imageInfo.layerCount = layers;
		imageInfo.type = type;

		m_DepthAttachments.emplace_back(imageInfo);
	}

}