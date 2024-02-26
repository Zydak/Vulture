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

		struct RenderPassCreateInfo
		{
			std::vector<VkSubpassDependency> Dependencies;

			std::vector<VkAttachmentLoadOp> LoadOP;
			std::vector<VkAttachmentStoreOp> StoreOP;
			std::vector<VkImageLayout> FinalLayouts;
		};

		struct CreateInfo
		{
			const std::vector<FramebufferAttachment>* AttachmentsFormats;
			VkRenderPass RenderPass = 0;
			VkExtent2D Extent = { 0, 0 };
			VkFormat DepthFormat = VK_FORMAT_MAX_ENUM;
			Image::ImageType Type = Image::ImageType::Image2D;
			VkImageUsageFlags CustomBits = 0;

			RenderPassCreateInfo* RenderPassInfo = nullptr;

			operator bool() const
			{
				return (!AttachmentsFormats->empty()) && (Extent.width != 0 || Extent.height != 0) && (DepthFormat != VK_FORMAT_MAX_ENUM);
			}
		};

		void Init(const CreateInfo& createInfo);
		void Destroy();

		void Bind(VkCommandBuffer cmd, const std::vector<VkClearValue>& clearColors);
		void Unbind(VkCommandBuffer cmd);

		inline VkRenderPass GetRenderPass() const { return m_RenderPass; }

		Framebuffer() = default;
		Framebuffer(const CreateInfo& createInfo);
		~Framebuffer();

		inline VkFramebuffer GetFramebufferHandle() const { return m_FramebufferHandle; }
		inline VkImageView GetImageView(int index) const { return m_Images[index].GetImageView(); }
		inline VkImage GetImage(int index) const { return m_Images[index].GetImage(); }
		inline const Image* GetImageNoVk(int index) const { return &m_Images[index]; }
		inline Image* GetImageNoVk(int index) { return &m_Images[index]; }
		
		inline uint32_t GetAttachmentCount() const { return (uint32_t)m_Images.size(); }
		
		inline std::vector<FramebufferAttachment> GetDepthAttachmentFormats() const { return m_AttachmentFormats; }

		inline glm::vec2 GetExtent() const { return { m_Extent.width, m_Extent.height }; }

	private:
		void CreateColorAttachment(VkFormat format, Image::ImageType type, VkImageUsageFlags customBits);
		void CreateDepthAttachment(Image::ImageType type, VkImageUsageFlags customBits);

		void CreateRenderPass(RenderPassCreateInfo* renderPassCreateInfo);

		VkFormat ToVkFormat(FramebufferAttachment format);

		VkExtent2D m_Extent;

		std::vector<VkImageView> m_Views;
		std::vector<FramebufferAttachment> m_AttachmentFormats;
		VkFramebuffer m_FramebufferHandle = VK_NULL_HANDLE;
		std::vector<Image> m_Images;

		VkFormat m_DepthFormat;

		bool m_Initialized = false;

		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		std::vector<VkImageLayout> m_FinalLayouts;
	};

}