#include "pch.h"
#include "Tonemap.h"

#include "Vulkan/Swapchain.h"
#include "Renderer/Renderer.h"

namespace Vulture
{

	void Tonemap::Init(const CreateInfo& info)
	{
		if (m_Initialized)
			Destroy();

		m_ImageSize = info.OutputImages[0]->GetImageSize();

		m_InputImages = info.InputImages;
		m_OutputImages = info.OutputImages;

		m_Descriptor.resize(m_OutputImages.size());
		for (int i = 0; i < m_OutputImages.size(); i++)
		{
			Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			m_Descriptor[i].Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
			m_Descriptor[i].AddImageSampler(
				0,
				info.InputImages[i]->GetSamplerHandle(),
				info.InputImages[i]->GetImageView(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);
			m_Descriptor[i].AddImageSampler(
				1,
				info.OutputImages[i]->GetSamplerHandle(),
				info.OutputImages[i]->GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL
			);
			m_Descriptor[i].Build();
		}

		// Pipeline
		{
			m_Push.Init({ VK_SHADER_STAGE_COMPUTE_BIT });

			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			info.ShaderFilepath = "../Vulture/src/Vulture/Shaders/spv/Tonemap.comp.spv";

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;
			info.PushConstants = m_Push.GetRangePtr();
			info.debugName = "Tone Map Pipeline";

			// Create the graphics pipeline
			m_Pipeline.Init(info);
		}

		m_Initialized = true;
	}

	void Tonemap::Destroy()
	{
		m_Pipeline.Destroy();
		
		for (int i = 0; i < m_InputImages.size(); i++)
		{
			m_Descriptor[i].Destroy();
		}
		m_Push.Destroy();

		m_Initialized = false;
	}

	Tonemap::Tonemap(const CreateInfo& info)
	{
		Init(info);
	}

	Tonemap::~Tonemap()
	{
		if (m_Initialized)
			Destroy();
	}

	void Tonemap::Run(const TonemapInfo& info, VkCommandBuffer cmd, uint32_t imageIndex /*= 0*/)
	{
		m_OutputImages[imageIndex]->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		m_InputImages[imageIndex]->TransitionImageLayout(
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			Vulture::Renderer::GetCurrentCommandBuffer(),
			VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
		);


		m_Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

		m_Descriptor[imageIndex].Bind(
			0,
			m_Pipeline.GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_COMPUTE, 
			cmd
		);

		m_Push.GetDataPtr()->Brightness = info.Brightness;
		m_Push.GetDataPtr()->Contrast	= info.Contrast;
		m_Push.GetDataPtr()->Exposure	= info.Exposure;
		m_Push.GetDataPtr()->Vignette	= info.Vignette;
		m_Push.GetDataPtr()->Saturation = info.Saturation;

		m_Push.Push(m_Pipeline.GetPipelineLayout(), cmd);

		vkCmdDispatch(cmd, ((int)m_ImageSize.width) / 8 + 1, ((int)m_ImageSize.width) / 8 + 1, 1);
	}

}