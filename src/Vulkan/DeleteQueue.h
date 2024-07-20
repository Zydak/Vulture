#pragma once

#include "pch.h"
#include "Pipeline.h"
#include "Image.h"
#include "DescriptorSet.h"

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

		static void UpdateQueue();

		static void TrashPipeline(const Pipeline& pipeline);
		static void TrashImage(Image& image);
		static void TrashBuffer(Buffer& buffer);
		static void TrashDescriptorSet(DescriptorSet&& set);
	private:
		static uint32_t s_FramesInFlight;

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

		};

		static std::vector<std::pair<PipelineInfo, uint32_t>> s_PipelineQueue;
		static std::vector<std::pair<ImageInfo, uint32_t>> s_ImageQueue;
		static std::vector<std::pair<BufferInfo, uint32_t>> s_BufferQueue;
		static std::vector<std::pair<DescriptorSet, uint32_t>> s_SetQueue;
	};
}