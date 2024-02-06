#include "pch.h"
#include "Utility/Utility.h"

#include "Framebuffer.h"
#include <vulkan/vulkan_core.h>

namespace Vulture
{
	/*
	 * @brief Initializes a Framebuffer object with color and depth attachments based on the specified
	 * createInfo.AttachmentsFormats, renderPass, extent, depthFormat, layers, type, and customBits parameters. It creates
	 * the required attachments and framebuffers accordingly.
	 *
	 * @param createInfo.AttachmentsFormats - Vector specifying the format of each attachment in the framebuffer.
	 * @param renderPass - The Vulkan render pass to be used with the framebuffer.
	 * @param extent - The extent (width and height) of the framebuffer.
	 * @param depthFormat - The Vulkan format for the depth attachment.
	 * @param layers - The number of layers in the framebuffer.
	 * @param type - The type of image attachments.
	 * @param customBits - Additional custom Vulkan image usage flags.
	 */
	void Framebuffer::Init(const CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		m_Extent = createInfo.Extent;
		m_DepthFormat = createInfo.DepthFormat;

		m_ColorAttachments.reserve(createInfo.AttachmentsFormats->size());
		m_DepthAttachments.reserve(createInfo.AttachmentsFormats->size());
		for (int i = 0; i < createInfo.AttachmentsFormats->size(); i++)
		{
			m_AttachmentFormats.push_back((*createInfo.AttachmentsFormats)[i]);
			switch ((*createInfo.AttachmentsFormats)[i])
			{
			case FramebufferAttachment::ColorRGBA32: CreateColorAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRGBA16: CreateColorAttachment(VK_FORMAT_R16G16B16A16_UNORM, createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRGBA8: CreateColorAttachment(VK_FORMAT_R8G8B8A8_UNORM, createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;

			case FramebufferAttachment::ColorRGB16: CreateColorAttachment(VK_FORMAT_R16G16B16_UNORM, createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRGB8: CreateColorAttachment(VK_FORMAT_R8G8B8_UNORM, createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;

			case FramebufferAttachment::ColorRG8: CreateColorAttachment(VK_FORMAT_R8G8_UNORM, createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRG16: CreateColorAttachment(VK_FORMAT_R16G16_UNORM, createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRG32: CreateColorAttachment(VK_FORMAT_R32G32_SFLOAT, createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;

			case FramebufferAttachment::ColorR8: CreateColorAttachment(VK_FORMAT_R8_UNORM, createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;

			case FramebufferAttachment::Depth: CreateDepthAttachment(createInfo.Layers, createInfo.Type, createInfo.CustomBits); break;
			}
		}

		for (int i = 0; i < m_ColorAttachments.size(); i++) m_Attachments.push_back(m_ColorAttachments[i].GetImageView());
		for (int i = 0; i < m_DepthAttachments.size(); i++) m_Attachments.push_back(m_DepthAttachments[i].GetImageView());

		{
			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = createInfo.RenderPass;
			framebufferInfo.attachmentCount = (uint32_t)m_Attachments.size();
			framebufferInfo.pAttachments = m_Attachments.data();
			framebufferInfo.width = m_Extent.width;
			framebufferInfo.height = m_Extent.height;
			framebufferInfo.layers = createInfo.Layers;

			VL_CORE_RETURN_ASSERT(vkCreateFramebuffer(Device::GetDevice(), &framebufferInfo, nullptr, &m_FramebufferHandle),
				VK_SUCCESS,
				"failed to create framebuffer!"
			);
		}

		if (createInfo.Layers > 1)
		{
			m_LayerFramebuffers.resize(createInfo.Layers);
			for (int i = 0; i < createInfo.Layers; i++)
			{
				m_Attachments.clear();
				for (int j = 0; j < m_ColorAttachments.size(); j++) m_Attachments.push_back(m_ColorAttachments[j].GetLayerView(i));
				for (int j = 0; j < m_DepthAttachments.size(); j++) m_Attachments.push_back(m_DepthAttachments[j].GetLayerView(i));
				VkFramebufferCreateInfo framebufferInfo = {};
				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = createInfo.RenderPass;
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

		m_Initialized = true;
	}

	void Framebuffer::Destroy()
	{
		vkDestroyFramebuffer(Device::GetDevice(), m_FramebufferHandle, nullptr);
		for (auto framebuffer : m_LayerFramebuffers)
		{
			vkDestroyFramebuffer(Device::GetDevice(), framebuffer, nullptr);
		}
	}

	/*
	 * @brief Initializes a Framebuffer object with color and depth attachments based on the specified
	 * createInfo.AttachmentsFormats, renderPass, extent, depthFormat, layers, type, and customBits parameters. It creates
	 * the required attachments and framebuffers accordingly.
	 *
	 * @param createInfo.AttachmentsFormats - Vector specifying the format of each attachment in the framebuffer.
	 * @param renderPass - The Vulkan render pass to be used with the framebuffer.
	 * @param extent - The extent (width and height) of the framebuffer.
	 * @param depthFormat - The Vulkan format for the depth attachment.
	 * @param layers - The number of layers in the framebuffer.
	 * @param type - The type of image attachments.
	 * @param customBits - Additional custom Vulkan image usage flags.
	 */
	Framebuffer::Framebuffer(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	Framebuffer::~Framebuffer()
	{
		if (m_Initialized)
			Destroy();
	}

	/**
	 * @brief Pushes a color attachment for the framebuffer using the specified format, layers, image type, and custom usage flags to m_ColorAttachments.
	 *
	 * @param format - The Vulkan format of the color attachment.
	 * @param layers - The number of layers in the color attachment.
	 * @param type - The image type of the color attachment.
	 * @param customBits - Additional custom Vulkan image usage flags.
	 */
	void Framebuffer::CreateColorAttachment(VkFormat format, int layers, Image::ImageType type, VkImageUsageFlags customBits)
	{
		Image::CreateInfo imageInfo;
		imageInfo.Width = m_Extent.width;
		imageInfo.Height = m_Extent.height;
		imageInfo.Format = format;
		imageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.Usage = customBits | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		imageInfo.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		imageInfo.SamplerInfo = SamplerInfo(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR);
		imageInfo.LayerCount = layers;
		imageInfo.Type = type;

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
	void Framebuffer::CreateDepthAttachment(int layers, Image::ImageType type, VkImageUsageFlags customBits)
	{
		customBits &= ~VK_IMAGE_USAGE_STORAGE_BIT; // Don't use storage bit on depth attachments
		Image::CreateInfo imageInfo;
		imageInfo.Width = m_Extent.width;
		imageInfo.Height = m_Extent.height;
		imageInfo.Format = m_DepthFormat;
		imageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.Usage = customBits | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		imageInfo.Aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageInfo.SamplerInfo = SamplerInfo(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR);
		imageInfo.LayerCount = layers;
		imageInfo.Type = type;

		m_DepthAttachments.emplace_back(imageInfo);
	}
}