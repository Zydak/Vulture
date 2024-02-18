#pragma once

#include "pch.h"
#include "Vulkan/Device.h"
#include "Vulkan/Image.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PushConstant.h"

namespace Vulture
{

	class Bloom
	{
	public:
		struct CreateInfo
		{
			std::vector<Ref<Image>> InputImages;
			std::vector<Ref<Image>> OutputImages;

			uint32_t mipsCount;
		};

		struct BloomInfo
		{
			float Threshold = 1.5f;
			float Strength = 0.5f;
			int MipCount = 6;
		};

		void Init(const CreateInfo& info);
		void Destroy();

		Bloom() = default;
		Bloom(const CreateInfo& info);
		~Bloom();

		void Run(const BloomInfo& bloomInfo, VkCommandBuffer cmd, uint32_t imageIndex = 0);

		void RecreateDescriptors(uint32_t mipsCount, int32_t frameIndex = -1);
	private:
		void CreateBloomMips();

		PushConstant<BloomInfo> m_Push;

		VkExtent2D m_ImageSize;

		std::vector<DescriptorSet> m_SeparateBrightValuesSet;
		std::vector<std::vector<DescriptorSet>> m_DownSampleSet;
		std::vector<std::vector<DescriptorSet>> m_AccumulateSet;

		std::vector<std::vector<Image>> m_BloomImages;

		Pipeline m_SeparateBrightValuesPipeline;
		Pipeline m_DownSamplePipeline;
		Pipeline m_AccumulatePipeline;

		std::vector<Ref<Image>> m_InputImages;
		std::vector<Ref<Image>> m_OutputImages;

		bool m_Initialized = false;
	};

}