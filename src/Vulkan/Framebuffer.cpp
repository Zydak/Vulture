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

		m_Images.reserve(createInfo.AttachmentsFormats->size());
		for (int i = 0; i < createInfo.AttachmentsFormats->size(); i++)
		{
			m_AttachmentFormats.push_back((*createInfo.AttachmentsFormats)[i]);
			switch ((*createInfo.AttachmentsFormats)[i])
			{
			case FramebufferAttachment::ColorRGBA32: CreateColorAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRGBA16: CreateColorAttachment(VK_FORMAT_R16G16B16A16_UNORM, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRGBA8: CreateColorAttachment(VK_FORMAT_R8G8B8A8_UNORM, createInfo.Type, createInfo.CustomBits); break;

			case FramebufferAttachment::ColorRGB16: CreateColorAttachment(VK_FORMAT_R16G16B16_UNORM, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRGB8: CreateColorAttachment(VK_FORMAT_R8G8B8_UNORM, createInfo.Type, createInfo.CustomBits); break;

			case FramebufferAttachment::ColorRG8: CreateColorAttachment(VK_FORMAT_R8G8_UNORM, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRG16: CreateColorAttachment(VK_FORMAT_R16G16_UNORM, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::ColorRG32: CreateColorAttachment(VK_FORMAT_R32G32_SFLOAT, createInfo.Type, createInfo.CustomBits); break;

			case FramebufferAttachment::ColorR8: CreateColorAttachment(VK_FORMAT_R8_UNORM, createInfo.Type, createInfo.CustomBits); break;

			case FramebufferAttachment::Depth32: CreateDepthAttachment(VK_FORMAT_D32_SFLOAT, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::Depth24: CreateDepthAttachment(VK_FORMAT_D24_UNORM_S8_UINT, createInfo.Type, createInfo.CustomBits); break;
			case FramebufferAttachment::Depth16: CreateDepthAttachment(VK_FORMAT_D16_UNORM, createInfo.Type, createInfo.CustomBits); break;
			}
		}

		for (int i = 0; i < m_Images.size(); i++) m_Views.push_back(m_Images[i]->GetImageView());

		if (createInfo.RenderPassInfo != nullptr)
		{
			CreateRenderPass(createInfo.RenderPassInfo);
		}

		{
			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_RenderPass == VK_NULL_HANDLE ? createInfo.RenderPass : m_RenderPass;
			framebufferInfo.attachmentCount = (uint32_t)m_Views.size();
			framebufferInfo.pAttachments = m_Views.data();
			framebufferInfo.width = m_Extent.width;
			framebufferInfo.height = m_Extent.height;
			framebufferInfo.layers = 1;

			VL_CORE_RETURN_ASSERT(vkCreateFramebuffer(Device::GetDevice(), &framebufferInfo, nullptr, &m_FramebufferHandle),
				VK_SUCCESS,
				"failed to create framebuffer!"
			);
		}

		m_Initialized = true;
	}

	void Framebuffer::Destroy()
	{
		vkDestroyFramebuffer(Device::GetDevice(), m_FramebufferHandle, nullptr);

		vkDestroyRenderPass(Device::GetDevice(), m_RenderPass, nullptr);
	}

	void Framebuffer::Bind(VkCommandBuffer cmd, const std::vector<VkClearValue>& clearColors)
	{
		VL_CORE_ASSERT(m_RenderPass != VK_NULL_HANDLE, "Render Pass Not Created! You have to pass RenderPassInfo into framebuffer create info to create one!");
		VL_CORE_ASSERT(m_FramebufferHandle != VK_NULL_HANDLE, "Framebuffer Not Created! You have to call Init() first!");

		// Set up viewport
		glm::vec2 extent = { m_Extent.width, m_Extent.height };
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = extent.x;
		viewport.height = extent.y;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// Set up scissor
		VkRect2D scissor{{0, 0}, VkExtent2D{(uint32_t)extent.x, (uint32_t)extent.y}};

		// Set viewport and scissor for the current command buffer
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		// Set up render pass information
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_RenderPass;
		renderPassInfo.framebuffer = m_FramebufferHandle;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = VkExtent2D{ (uint32_t)extent.x, (uint32_t)extent.y };
		renderPassInfo.clearValueCount = (uint32_t)clearColors.size();
		renderPassInfo.pClearValues = clearColors.data();

		// Begin the render pass for the current command buffer
		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		for (int i = 0; i < m_Images.size(); i++)
		{
			m_Images[i]->SetLayout(m_FinalLayouts[i]); // change layouts
		}
	}

	void Framebuffer::Unbind(VkCommandBuffer cmd)
	{
		vkCmdEndRenderPass(cmd);
	}

	void Framebuffer::TransitionImageLayout(VkImageLayout newLayout, VkCommandBuffer cmd, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
	{
		for (int i = 0; i < m_Images.size(); i++)
		{
			m_Images[i]->TransitionImageLayout(newLayout, cmd, srcAccess, dstAccess, srcStage, dstStage);
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
	 * @brief Pushes a color attachment for the framebuffer using the specified format, image type, and custom usage flags to m_ColorAttachments.
	 *
	 * @param format - The Vulkan format of the color attachment.
	 * @param type - The image type of the color attachment.
	 * @param customBits - Additional custom Vulkan image usage flags.
	 */
	void Framebuffer::CreateColorAttachment(VkFormat format, Image::ImageType type, VkImageUsageFlags customBits)
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
		imageInfo.LayerCount = 1;
		imageInfo.Type = type;

		m_Images.emplace_back(std::make_shared<Image>(imageInfo));
	}

	/**
	 * @brief Pushes a depth attachment for the framebuffer using the specified format, image type, and custom usage flags to m_DepthAttachments.
	 *
	 * @param format - The Vulkan format of the color attachment.
	 * @param type - The image type of the color attachment.
	 * @param customBits - Additional custom Vulkan image usage flags.
	 */
	void Framebuffer::CreateDepthAttachment(VkFormat format, Image::ImageType type, VkImageUsageFlags customBits)
	{
		VkFormat depthFormat = Device::FindSupportedFormat({ format, VK_FORMAT_D16_UNORM, VK_FORMAT_D32_SFLOAT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
		//customBits &= ~VK_IMAGE_USAGE_STORAGE_BIT; // Don't use storage bit on depth attachments
		Image::CreateInfo imageInfo;
		imageInfo.Width = m_Extent.width;
		imageInfo.Height = m_Extent.height;
		imageInfo.Format = depthFormat;
		imageInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.Usage = customBits | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.Properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		imageInfo.Aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		imageInfo.SamplerInfo = SamplerInfo(VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR);
		imageInfo.LayerCount = 1;
		imageInfo.Type = type;

		m_Images.emplace_back(std::make_shared<Image>(imageInfo));
	}

	void Framebuffer::CreateRenderPass(RenderPassCreateInfo* renderPassCreateInfo)
	{
		bool useFinalLayouts = false;
		bool useLoadOps = false;
		bool useStoreOps = false;
		if (!renderPassCreateInfo->FinalLayouts.empty())
		{
			VL_CORE_ASSERT(renderPassCreateInfo->FinalLayouts.size() < m_AttachmentFormats.size(),
				"You have to specify final layout for all attachments"
			);
			useFinalLayouts = true;
		}
		if (!renderPassCreateInfo->LoadOP.empty())
		{
			VL_CORE_ASSERT(renderPassCreateInfo->LoadOP.size() < m_AttachmentFormats.size(),
				"You have to specify LoadOp for all attachments"
			);
			useLoadOps = true;
		}
		if (!renderPassCreateInfo->StoreOP.empty())
		{
			VL_CORE_ASSERT(renderPassCreateInfo->StoreOP.size() < m_AttachmentFormats.size(),
				"You have to specify StoreOp for all attachments"
			);
			useStoreOps = true;
		}

		VkAttachmentReference depthReference;

		std::vector<VkAttachmentDescription> descriptions;
		std::vector<VkAttachmentReference> references;

		uint32_t depthCount = 0;
		for (int i = 0; i < m_AttachmentFormats.size(); i++)
		{
			VL_CORE_ASSERT(depthCount <= 1, "Can't have multiple depth attachments!");

			VkAttachmentDescription description = {};
			description.format = ToVkFormat(m_AttachmentFormats[i]);
			description.samples = VK_SAMPLE_COUNT_1_BIT;
			description.loadOp = useLoadOps ? renderPassCreateInfo->LoadOP[i] : VK_ATTACHMENT_LOAD_OP_CLEAR;
			description.storeOp = useStoreOps ? renderPassCreateInfo->StoreOP[i] : VK_ATTACHMENT_STORE_OP_STORE;
			description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			if (m_AttachmentFormats[i] == FramebufferAttachment::Depth32 || m_AttachmentFormats[i] == FramebufferAttachment::Depth24 || m_AttachmentFormats[i] == FramebufferAttachment::Depth16)
			{
				description.finalLayout = useFinalLayouts ? renderPassCreateInfo->FinalLayouts[i] : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				m_FinalLayouts.push_back(useFinalLayouts ? renderPassCreateInfo->FinalLayouts[i] : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
			}
			else
			{
				description.finalLayout = useFinalLayouts ? renderPassCreateInfo->FinalLayouts[i] : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				m_FinalLayouts.push_back(useFinalLayouts ? renderPassCreateInfo->FinalLayouts[i] : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}

			descriptions.push_back(description);

			// ----------------------------------------------------------------------

			VkAttachmentReference reference = {};
			reference.attachment = i;
			if (m_AttachmentFormats[i] == FramebufferAttachment::Depth32 || m_AttachmentFormats[i] == FramebufferAttachment::Depth24 || m_AttachmentFormats[i] == FramebufferAttachment::Depth16)
			{
				reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

				depthReference = reference;
				depthCount++;
			}
			else
			{
				reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

				references.push_back(reference);
			}
		}

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = (uint32_t)references.size();
		subpass.pColorAttachments = references.data();
		subpass.pDepthStencilAttachment = depthCount > 0 ? &depthReference : nullptr;

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = (uint32_t)descriptions.size();
		renderPassInfo.pAttachments = descriptions.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = (uint32_t)renderPassCreateInfo->Dependencies.size();
		renderPassInfo.pDependencies = renderPassCreateInfo->Dependencies.data();

		VL_CORE_RETURN_ASSERT(vkCreateRenderPass(Device::GetDevice(), &renderPassInfo, nullptr, &m_RenderPass), 0, "Failed to create render pass!");
	}

	VkFormat Framebuffer::ToVkFormat(FramebufferAttachment format)
	{
		switch (format)
		{
		case FramebufferAttachment::ColorRGBA32: return VK_FORMAT_R32G32B32A32_SFLOAT;
		case FramebufferAttachment::ColorRGBA16: return VK_FORMAT_R16G16B16A16_UNORM;
		case FramebufferAttachment::ColorRGBA8: return VK_FORMAT_R8G8B8A8_UNORM;

		case FramebufferAttachment::ColorRGB16: return VK_FORMAT_R16G16B16_UNORM;
		case FramebufferAttachment::ColorRGB8: return VK_FORMAT_R8G8B8_UNORM;

		case FramebufferAttachment::ColorRG8: return VK_FORMAT_R8G8_UNORM;
		case FramebufferAttachment::ColorRG16: return VK_FORMAT_R16G16_UNORM;
		case FramebufferAttachment::ColorRG32: return VK_FORMAT_R32G32_SFLOAT;

		case FramebufferAttachment::ColorR8: return VK_FORMAT_R8_UNORM;

		case FramebufferAttachment::Depth32: return VK_FORMAT_D32_SFLOAT;
		case FramebufferAttachment::Depth24: return VK_FORMAT_D24_UNORM_S8_UINT;
		case FramebufferAttachment::Depth16: return VK_FORMAT_D16_UNORM;

		default:
			VL_CORE_ASSERT(false, "Format not supported");
			return VK_FORMAT_MAX_ENUM; // just to get rid of the warning
		}
	}

}