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

			uint32_t MipsCount = 10;
		};

		struct BloomInfo
		{
			float Threshold = 1.5f;
			float Strength = 0.8f;
			int MipCount = 10;
		};

		void Init(const CreateInfo& info);
		void Destroy();

		Bloom() = default;
		Bloom(const CreateInfo& info);
		~Bloom();

		Bloom(const Bloom& other) = delete;
		Bloom& operator=(const Bloom& other) = delete;
		Bloom(Bloom&& other) noexcept;
		Bloom& operator=(Bloom&& other) noexcept;

		void Run(const BloomInfo& bloomInfo, VkCommandBuffer cmd);

		void UpdateDescriptors(const CreateInfo& info);
		void RecreateDescriptors(uint32_t mipsCount);
		void CreateBloomMips();

		inline bool IsInitialized() const { return m_Initialized; }
	private:

		PushConstant<BloomInfo> m_Push;

		VkExtent2D m_ImageSize = { 0, 0 };

		DescriptorSet m_SeparateBrightValuesSet;
		std::vector<DescriptorSet> m_DownSampleSet;
		std::vector<DescriptorSet> m_AccumulateSet;

		std::vector<Image> m_BloomImages;

		Pipeline m_SeparateBrightValuesPipeline;
		Pipeline m_DownSamplePipeline;
		Pipeline m_AccumulatePipeline;

		Image* m_InputImage = nullptr;
		Image* m_OutputImage = nullptr;

		bool m_Initialized = false;

		void Reset();
	};

}