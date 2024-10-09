#pragma once

#include "pch.h"
#include "Pipeline.h"
#include "Image.h"
#include "DescriptorSet.h"
#include "Framebuffer.h"

namespace Vulture
{
	class DeleteQueue
	{
	public:
		DeleteQueue() = delete;
		~DeleteQueue() = delete;

		struct CreateInfo
		{
			uint32_t FramesInFlight;
		};

		static void Init(const CreateInfo& info);
		static void Destroy();

		static void ClearQueue();

		static void UpdateQueue();

		static void TrashPipeline(const Pipeline& pipeline);
		static void TrashImage(Image& image);
		static void TrashBuffer(Buffer& buffer);
		static void TrashDescriptorSetLayout(DescriptorSetLayout& set);
		static void TrashRenderPass(VkRenderPass renderPass);
		static void TrashFramebuffer(VkFramebuffer framebuffer);
	private:

		struct PipelineInfo
		{
			VkPipeline Handle;
			VkPipelineLayout Layout;
		};

		struct ImageInfo
		{
			VkImage Handle;
			std::vector<VkImageView> Views;
			VmaAllocation* Allocation;
		};

		struct BufferInfo
		{
			VkBuffer Handle;
			VmaAllocation* Allocation;
			VmaPool* Pool;
		};

		struct DescriptorInfo
		{
			VkDescriptorSetLayout DescriptorSetLayoutHandle;
		};

		inline static uint32_t s_FramesInFlight = 0;

		inline static std::vector<std::pair<PipelineInfo, uint32_t>> s_PipelineQueue;
		inline static std::vector<std::pair<ImageInfo, uint32_t>> s_ImageQueue;
		inline static std::vector<std::pair<BufferInfo, uint32_t>> s_BufferQueue;
		inline static std::vector<std::pair<DescriptorInfo, uint32_t>> s_SetQueue;
		inline static std::vector<std::pair<VkRenderPass, uint32_t>> s_RenderPassQueue;
		inline static std::vector<std::pair<VkFramebuffer, uint32_t>> s_FramebufferQueue;

		inline static std::mutex s_Mutex;
	};
}