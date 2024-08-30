// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "pch.h"

#include "Device.h"
#include "Image.h"

#include <vulkan/vulkan.h>

namespace Vulture
{
	enum class FramebufferAttachment
	{
		Depth32,
		Depth24,
		Depth16,

		ColorRGBA8,
		ColorRGBA16,
		ColorRGB8,
		ColorRGB16,
		ColorRGBA32,
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
			std::vector<VkSubpassDependency> Dependencies = {};

			std::vector<VkAttachmentLoadOp> LoadOP = {};
			std::vector<VkAttachmentStoreOp> StoreOP = {};
			std::vector<VkImageLayout> FinalLayouts = {};
		};

		struct CreateInfo
		{
			std::vector<FramebufferAttachment> AttachmentsFormats;
			VkRenderPass RenderPass = 0;
			VkExtent2D Extent = { 0, 0 };
			Image::ImageType Type = Image::ImageType::Image2D;
			VkImageUsageFlags CustomBits = 0;

			RenderPassCreateInfo* RenderPassInfo = nullptr;

			operator bool() const
			{
				return (!AttachmentsFormats.empty()) && (Extent.width != 0 || Extent.height != 0);
			}
		};

		void Init(const CreateInfo& createInfo);
		void Destroy();

		void Bind(VkCommandBuffer cmd, const std::vector<VkClearValue>& clearColors);
		void Unbind(VkCommandBuffer cmd);

		void TransitionImageLayout(VkImageLayout newLayout, VkCommandBuffer cmd = 0);

		Framebuffer() = default;
		Framebuffer(const CreateInfo& createInfo);
		~Framebuffer();

		Framebuffer(const Framebuffer& other) = delete;
		Framebuffer& operator=(const Framebuffer& other) = delete;
		Framebuffer(Framebuffer&& other) noexcept;
		Framebuffer& operator=(Framebuffer&& other) noexcept;

		inline VkRenderPass GetRenderPass() const { return m_RenderPass; }
		inline VkFramebuffer GetFramebufferHandle() const { return m_FramebufferHandle; }
		inline VkImageView GetImageView(int index) const { return m_Images[index]->GetImageView(); }
		inline VkImage GetImage(int index) const { return m_Images[index]->GetImage(); }
		inline Ref<Image> GetImageNoVk(int index) const { return m_Images[index]; }
		inline Ref<Image> GetImageNoVk(int index) { return m_Images[index]; }
		
		inline uint32_t GetAttachmentCount() const { return (uint32_t)m_Images.size(); }
		
		inline std::vector<FramebufferAttachment> GetDepthAttachmentFormats() const { return m_AttachmentFormats; }

		inline glm::vec2 GetExtent() const { return { m_Extent.width, m_Extent.height }; }

		inline bool IsInitialized() const { return m_Initialized; }

	private:
		void CreateColorAttachment(VkFormat format, Image::ImageType type, VkImageUsageFlags customBits);
		void CreateDepthAttachment(VkFormat format, Image::ImageType type, VkImageUsageFlags customBits);

		void CreateRenderPass(RenderPassCreateInfo* renderPassCreateInfo);

		VkFormat ToVkFormat(FramebufferAttachment format);

		VkExtent2D m_Extent;

		std::vector<VkImageView> m_Views;
		std::vector<FramebufferAttachment> m_AttachmentFormats;
		VkFramebuffer m_FramebufferHandle = VK_NULL_HANDLE;
		std::vector<Ref<Image>> m_Images;

		bool m_Initialized = false;

		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		std::vector<VkImageLayout> m_FinalLayouts;

		void Reset();
	};

}