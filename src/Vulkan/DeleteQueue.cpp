#include "pch.h"
#include "DeleteQueue.h"

namespace Vulture
{

	void DeleteQueue::Init(const CreateInfo& info)
	{
		s_FramesInFlight = info.FramesInFlight;
	}

	void DeleteQueue::Destroy()
	{
		s_FramesInFlight = 0;

		s_PipelineQueue.clear();
	}

	void DeleteQueue::UpdateQueue()
	{
		// Pipelines
		for (int i = 0; i < s_PipelineQueue.size(); i++)
		{
			if (s_PipelineQueue[i].second == 0)
			{
				s_PipelineQueue[i].first.Destroy();
				s_PipelineQueue.erase(s_PipelineQueue.begin() + i);
				i = 0; // Go back to the beginning of the vector
			}
			else
			{
				s_PipelineQueue[i].second--;
			}
		}

		// Descriptor Sets
		for (int i = 0; i < s_SetQueue.size(); i++)
		{
			if (s_SetQueue[i].second == 0)
			{
				s_SetQueue[i].first.Destroy();
				s_SetQueue.erase(s_SetQueue.begin() + i);
				i = 0; // Go back to the beginning of the vector
			}
			else
			{
				s_SetQueue[i].second--;
			}
		}
	}

	void DeleteQueue::TrashPipeline(Pipeline&& pipeline)
	{
		s_PipelineQueue.push_back(std::make_pair(std::move(pipeline), s_FramesInFlight));
	}

	void DeleteQueue::TrashDescriptorSet(DescriptorSet&& set)
	{
		s_SetQueue.push_back(std::make_pair(std::move(set), s_FramesInFlight));
	}

	uint32_t DeleteQueue::s_FramesInFlight = 0;

	std::vector<std::pair<Vulture::Pipeline, uint32_t>> DeleteQueue::s_PipelineQueue;
	std::vector<std::pair<Vulture::DescriptorSet, uint32_t>> DeleteQueue::s_SetQueue;

}