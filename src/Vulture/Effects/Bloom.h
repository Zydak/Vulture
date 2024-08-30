// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

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
			Image* InputImage = nullptr;
			Image* OutputImage = nullptr;
		};

		struct BloomInfo
		{
			float Threshold = 1.5f;
			float Strength = 0.4f;
			uint32_t MipCount = 10;
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

		inline bool IsInitialized() const { return m_Initialized; }
	private:
		void RecreateDescriptors(uint32_t mipsCount);
		void CreateBloomMips();
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

		uint32_t m_CurrentMipCount = 0;

		bool m_Initialized = false;

		void Reset();
	};

}