#pragma once

#include "pch.h"
#include "Pipeline.h"
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

		static void TrashPipeline(Pipeline&& pipeline);
		static void TrashDescriptorSet(DescriptorSet&& set);
	private:
		static uint32_t s_FramesInFlight;

		static std::vector<std::pair<Pipeline, uint32_t>> s_PipelineQueue;
		static std::vector<std::pair<DescriptorSet, uint32_t>> s_SetQueue;
	};
}