#pragma once

#include "pch.h"
#include "Vulkan/Device.h"
#include "Vulkan/Image.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/PushConstant.h"

namespace Vulture
{
	class Tonemap
	{
	public:
		struct CreateInfo
		{
			std::vector<Ref<Image>> InputImages;
			std::vector<Ref<Image>> OutputImages;
		};

		struct TonemapInfo
		{
			float Contrast = 1.0f;
			float Saturation = 1.0f;
			float Exposure = 1.0f;
			float Brightness = 1.0f;
			float Vignette = 0.0f;
		};

		void Init(const CreateInfo& info);
		void Destroy();

		Tonemap() = default;
		Tonemap(const CreateInfo& info);
		~Tonemap();

		void Run(const TonemapInfo& info, VkCommandBuffer cmd, uint32_t imageIndex = 0);
	private:
		std::vector<DescriptorSet> m_Descriptor;
		Pipeline m_Pipeline;
		PushConstant<TonemapInfo> m_Push;

		VkExtent2D m_ImageSize;

		std::vector<Ref<Image>> m_InputImages;
		std::vector<Ref<Image>> m_OutputImages;

		bool m_Initialized = false;
	};
}