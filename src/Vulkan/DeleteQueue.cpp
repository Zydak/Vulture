// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

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
		for (int i = 0; i < (int)s_FramesInFlight + 1; i++)
		{
			UpdateQueue();
		}

		s_FramesInFlight = 0;
	}

	void DeleteQueue::UpdateQueue()
	{
		// Pipelines
		for (int i = 0; i < s_PipelineQueue.size(); i++)
		{
			if (s_PipelineQueue[i].second == 0)
			{
				vkDestroyPipeline(Device::GetDevice(), s_PipelineQueue[i].first.Handle, nullptr);
				vkDestroyPipelineLayout(Device::GetDevice(), s_PipelineQueue[i].first.Layout, nullptr);

				s_PipelineQueue.erase(s_PipelineQueue.begin() + i);
				i = -1; // Go back to the beginning of the vector
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
				vkDestroyDescriptorSetLayout(Device::GetDevice(), s_SetQueue[i].first.DescriptorSetLayoutHandle, nullptr);

				s_SetQueue.erase(s_SetQueue.begin() + i);
				i = -1; // Go back to the beginning of the vector
			}
			else
			{
				s_SetQueue[i].second--;
			}
		}

		// Images
		for (int i = 0; i < s_ImageQueue.size(); i++)
		{
			if (s_ImageQueue[i].second == 0)
			{
				for (auto view : s_ImageQueue[i].first.Views)
				{
					vkDestroyImageView(Device::GetDevice(), view, nullptr);
				}

				vmaDestroyImage(Device::GetAllocator(), s_ImageQueue[i].first.Handle, *s_ImageQueue[i].first.Allocation);

				delete s_ImageQueue[i].first.Allocation;

				s_ImageQueue.erase(s_ImageQueue.begin() + i);
				i = -1; // Go back to the beginning of the vector
			}
			else
			{
				s_ImageQueue[i].second--;
			}
		}

		// Buffers
		for (int i = 0; i < s_BufferQueue.size(); i++)
		{
			auto& buf = s_BufferQueue[i];
			if (buf.second == 0)
			{
				// Destroy the Vulkan buffer and deallocate the buffer memory.
				vmaDestroyBuffer(Device::GetAllocator(), buf.first.Handle, *buf.first.Allocation);

				if (buf.first.Pool != nullptr)
				{
					vmaDestroyPool(Device::GetAllocator(), *buf.first.Pool);
				}

				delete buf.first.Allocation;

				s_BufferQueue.erase(s_BufferQueue.begin() + i);
				i = -1; // Go back to the beginning of the vector
			}
			else
			{
				buf.second--;
			}
		}
	}

	void DeleteQueue::TrashPipeline(const Pipeline& pipeline)
	{
		PipelineInfo info{};
		info.Handle = pipeline.GetPipeline();
		info.Layout = pipeline.GetPipelineLayout();

		s_PipelineQueue.emplace_back(std::make_pair(info, s_FramesInFlight));
	}

	void DeleteQueue::TrashImage(Image& image)
	{
		ImageInfo info{};
		info.Handle = image.GetImage();
		info.Views = image.GetImageViews();
		info.Allocation = image.GetAllocation();

		s_ImageQueue.emplace_back(std::make_pair(info, s_FramesInFlight));
	}

	void DeleteQueue::TrashBuffer(Buffer& buffer)
	{
		BufferInfo info{};
		info.Handle = buffer.GetBuffer();
		info.Allocation = buffer.GetAllocation();
		info.Pool = buffer.GetVmaPool();

		s_BufferQueue.emplace_back(std::make_pair(info, s_FramesInFlight));
	}

	void DeleteQueue::TrashDescriptorSetLayout(DescriptorSetLayout& set)
	{
		DescriptorInfo info{};
		info.DescriptorSetLayoutHandle = set.GetDescriptorSetLayoutHandle();

		s_SetQueue.emplace_back(std::make_pair(info, s_FramesInFlight));
	}

	uint32_t DeleteQueue::s_FramesInFlight = 0;

	std::vector<std::pair<Vulture::DeleteQueue::PipelineInfo, uint32_t>> DeleteQueue::s_PipelineQueue;
	std::vector<std::pair<Vulture::DeleteQueue::ImageInfo, uint32_t>> DeleteQueue::s_ImageQueue;
	std::vector<std::pair<Vulture::DeleteQueue::BufferInfo, uint32_t>> DeleteQueue::s_BufferQueue;
	std::vector<std::pair<Vulture::DeleteQueue::DescriptorInfo, uint32_t>> DeleteQueue::s_SetQueue;

}