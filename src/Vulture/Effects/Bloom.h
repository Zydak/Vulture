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
			Image* InputImage;
			Image* OutputImage;

			uint32_t MipsCount = 6;
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

		void Run(const BloomInfo& bloomInfo, VkCommandBuffer cmd);

		void UpdateDescriptors(const CreateInfo& info);
		void RecreateDescriptors(uint32_t mipsCount);
		void CreateBloomMips();
	private:

		PushConstant<BloomInfo> m_Push;

		VkExtent2D m_ImageSize;

		DescriptorSet m_SeparateBrightValuesSet;
		std::vector<DescriptorSet> m_DownSampleSet;
		std::vector<DescriptorSet> m_AccumulateSet;

		std::vector<Image> m_BloomImages;

		Pipeline m_SeparateBrightValuesPipeline;
		Pipeline m_DownSamplePipeline;
		Pipeline m_AccumulatePipeline;

		Image* m_InputImage;
		Image* m_OutputImage;

		bool m_Initialized = false;
	};

}