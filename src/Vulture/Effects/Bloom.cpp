#include "pch.h"
#include "Bloom.h"

#include "Renderer/Renderer.h"

namespace Vulture
{

	void Bloom::Init(const CreateInfo& info)
	{
		if (m_Initialized)
			Destroy();

		m_InputImages = info.InputImages;
		m_OutputImages = info.OutputImages;

		//-----------------------------------------------
		// Pipelines
		//-----------------------------------------------

		m_Push.Init({ VK_SHADER_STAGE_COMPUTE_BIT });
		// Bloom Separate Bright Values
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			Shader shader({ "../Vulture/src/Vulture/Shaders/SeparateBrightValues.comp" , VK_SHADER_STAGE_COMPUTE_BIT });
			info.Shader = &shader;
			info.PushConstants = m_Push.GetRangePtr();

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;
			info.debugName = "Bloom Separate Bright Values Pipeline";

			// Create the graphics pipeline
			m_SeparateBrightValuesPipeline.Init(info);
		}

		// Bloom Accumulate
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			Shader shader({ "../Vulture/src/Vulture/Shaders/Bloom.comp" , VK_SHADER_STAGE_COMPUTE_BIT });
			info.Shader = &shader;
			info.PushConstants = m_Push.GetRangePtr();

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;
			info.debugName = "Bloom Accumulate Pipeline";

			// Create the graphics pipeline
			m_AccumulatePipeline.Init(info);
		}

		// Bloom Down Sample
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			Pipeline::ComputeCreateInfo info{};
			Shader shader({ "../Vulture/src/Vulture/Shaders/BloomDownSample.comp" , VK_SHADER_STAGE_COMPUTE_BIT });
			info.Shader = &shader;
			info.PushConstants = m_Push.GetRangePtr();

			// Descriptor set layouts for the pipeline
			std::vector<VkDescriptorSetLayout> layouts
			{
				imageLayout.GetDescriptorSetLayoutHandle()
			};
			info.DescriptorSetLayouts = layouts;
			info.debugName = "Bloom Down Sample Pipeline";

			// Create the graphics pipeline
			m_DownSamplePipeline.Init(info);
		}

		CreateBloomMips();
		RecreateDescriptors(info.MipsCount);

		m_Initialized = true;
	}

	void Bloom::Destroy()
	{
		m_SeparateBrightValuesSet.clear();
		m_DownSampleSet.clear();
		m_AccumulateSet.clear();

		m_AccumulatePipeline.Destroy();
		m_SeparateBrightValuesPipeline.Destroy();
		m_DownSamplePipeline.Destroy();

		m_BloomImages.clear();

		m_Initialized = false;
	}

	Bloom::Bloom(const CreateInfo& info)
	{
		Init(info);
	}

	Bloom::~Bloom()
	{
		if (m_Initialized)
			Destroy();
	}

	void Bloom::Run(const BloomInfo& bloomInfo, VkCommandBuffer cmd, uint32_t imageIndex /*= 0*/)
	{
		if (m_InputImages[imageIndex]->GetImage() != m_OutputImages[imageIndex]->GetImage())
		{
			VkImageLayout prevInputLayout = m_InputImages[imageIndex]->GetLayout();
			m_InputImages[imageIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmd);
			m_OutputImages[imageIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmd);
			VkImageBlit region{};
			region.dstOffsets[0] = { 0, 0, 0 };
			region.dstOffsets[1] = { (int)m_OutputImages[imageIndex]->GetImageSize().width, (int)m_OutputImages[imageIndex]->GetImageSize().height, 1 };
			region.srcOffsets[0] = { 0, 0, 0 };
			region.srcOffsets[1] = { (int)m_InputImages[imageIndex]->GetImageSize().width, (int)m_InputImages[imageIndex]->GetImageSize().height, 1 };
			region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			vkCmdBlitImage(cmd, m_InputImages[imageIndex]->GetImage(), m_InputImages[imageIndex]->GetLayout(), m_OutputImages[imageIndex]->GetImage(), m_OutputImages[imageIndex]->GetLayout(), 1, &region, VK_FILTER_LINEAR);
			m_OutputImages[imageIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);
			m_InputImages[imageIndex]->TransitionImageLayout(prevInputLayout, cmd);
		}

		if (bloomInfo.MipCount <= 0 || bloomInfo.MipCount > 10)
		{
			VL_CORE_WARN("Incorrect mips count! {}. Min = 1 & Max = 10", bloomInfo.MipCount);
			return;
		}

		m_Push.GetDataPtr()->MipCount = bloomInfo.MipCount;
		m_Push.GetDataPtr()->Strength = bloomInfo.Strength;
		m_Push.GetDataPtr()->Threshold = bloomInfo.Threshold;

		m_BloomImages[imageIndex][0].TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, cmd);

		m_SeparateBrightValuesPipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
		m_SeparateBrightValuesSet[imageIndex].Bind(0, m_SeparateBrightValuesPipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd);

		m_Push.Push(m_SeparateBrightValuesPipeline.GetPipelineLayout(), cmd);

		vkCmdDispatch(cmd, m_BloomImages[imageIndex][0].GetImageSize().width / 8 + 1, m_BloomImages[imageIndex][0].GetImageSize().height / 8 + 1, 1);

		m_BloomImages[imageIndex][0].TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);

		for (int i = 1; i < (int)bloomInfo.MipCount + 1; i++)
		{
			m_BloomImages[imageIndex][i].TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, cmd);
		}

		m_DownSamplePipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
		m_Push.Push(m_AccumulatePipeline.GetPipelineLayout(), cmd);
		for (int i = 1; i < (int)bloomInfo.MipCount + 1; i++)
		{
			m_DownSampleSet[imageIndex][i - 1].Bind(0, m_DownSamplePipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd);

			vkCmdDispatch(cmd, m_BloomImages[imageIndex][i].GetImageSize().width / 8 + 1, m_BloomImages[imageIndex][i].GetImageSize().height / 8 + 1, 1);

			m_BloomImages[imageIndex][i].TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);
		}

		m_AccumulatePipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
		m_Push.Push(m_AccumulatePipeline.GetPipelineLayout(), cmd);

		int i;
		int idx;
		for (i = 0; i < (int)bloomInfo.MipCount; i++)
		{
			idx = bloomInfo.MipCount - i;

			if (idx != bloomInfo.MipCount)
			{
				m_BloomImages[imageIndex][idx].TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmd);
			}

			m_BloomImages[imageIndex][idx - 1].TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, cmd);

			m_AccumulateSet[imageIndex][i].Bind(0, m_AccumulatePipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd);

			vkCmdDispatch(cmd, m_BloomImages[imageIndex][idx - 1].GetImageSize().width / 8 + 1, m_BloomImages[imageIndex][idx - 1].GetImageSize().height / 8 + 1, 1);
		}

		m_OutputImages[imageIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_GENERAL, cmd);

		m_AccumulateSet[imageIndex][i].Bind(0, m_AccumulatePipeline.GetPipelineLayout(), VK_PIPELINE_BIND_POINT_COMPUTE, cmd);

		vkCmdDispatch(cmd, m_InputImages[imageIndex]->GetImageSize().width / 8 + 1, m_InputImages[imageIndex]->GetImageSize().height / 8 + 1, 1);
	}

	void Bloom::UpdateDescriptors(const CreateInfo& info)
	{
		m_InputImages = info.InputImages;
		m_OutputImages = info.OutputImages;

		for (int i = 0; i < m_OutputImages.size(); i++)
		{
			m_SeparateBrightValuesSet[i].UpdateImageSampler(
				0,
				{ Vulture::Renderer::GetLinearSamplerHandle(), // input image is copied to output at the start of bloom pass
				m_OutputImages[i]->GetImageView(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			);

			m_AccumulateSet[i][info.MipsCount].AddImageSampler(
				1,
				{ Vulture::Renderer::GetLinearSamplerHandle(),
				m_OutputImages[i]->GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL }
			);
		}
	}

	void Bloom::RecreateDescriptors(uint32_t mipsCount, int32_t frameIndex)
	{
		if (mipsCount <= 0 || mipsCount > 10)
		{
			VL_CORE_WARN("Incorrect mips count! {}. Min = 1 & Max = 10", mipsCount);
			return;
		}

		uint32_t loopCount = (uint32_t)m_OutputImages.size();
		if (frameIndex != -1)
		{
			loopCount = 1;
		}
		else
		{
			frameIndex = 0;
		}

		//-----------------------------------------------
		// Descriptor sets
		//-----------------------------------------------

		// Bloom Separate Bright Values
		m_SeparateBrightValuesSet.resize(m_InputImages.size());
		for (uint32_t i = frameIndex; i < loopCount + frameIndex; i++)
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			m_SeparateBrightValuesSet[i].Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
			m_SeparateBrightValuesSet[i].AddImageSampler(
				0,
				{ Vulture::Renderer::GetLinearSamplerHandle(), // input image is copied to output at the start of bloom pass
				m_OutputImages[i]->GetImageView(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			);
			m_SeparateBrightValuesSet[i].AddImageSampler(
				1,
				{ Vulture::Renderer::GetLinearSamplerHandle(),
				m_BloomImages[i][0].GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL }
			);
			m_SeparateBrightValuesSet[i].Build();
		}

		// Bloom Accumulate
		m_AccumulateSet.resize(m_InputImages.size());
		for (uint32_t i = frameIndex; i < loopCount + frameIndex; i++)
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			m_AccumulateSet[i].clear();
			m_AccumulateSet[i].resize(mipsCount + 1);
			int j;
			int descIdx;
			for (j = 0; j < m_AccumulateSet[i].size() - 1; j++)
			{
				descIdx = (mipsCount)-j;
				m_AccumulateSet[i][j].Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });

				m_AccumulateSet[i][j].AddImageSampler(0, { Vulture::Renderer::GetLinearSamplerHandle(), m_BloomImages[i][descIdx].GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
				m_AccumulateSet[i][j].AddImageSampler(1, { Vulture::Renderer::GetLinearSamplerHandle(), m_BloomImages[i][descIdx - 1].GetImageView(), VK_IMAGE_LAYOUT_GENERAL });
				m_AccumulateSet[i][j].Build();
			}
			m_AccumulateSet[i][j].Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });

			m_AccumulateSet[i][j].AddImageSampler(0, { Vulture::Renderer::GetLinearSamplerHandle(), m_BloomImages[i][descIdx].GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
			m_AccumulateSet[i][j].AddImageSampler(1, { Vulture::Renderer::GetLinearSamplerHandle(), m_OutputImages[i]->GetImageView(), VK_IMAGE_LAYOUT_GENERAL });
			m_AccumulateSet[i][j].Build();
		}

		// Bloom Down Sample
		m_DownSampleSet.resize(m_InputImages.size());
		for (uint32_t i = frameIndex; i < loopCount + frameIndex; i++)
		{
			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			m_DownSampleSet[i].clear();
			m_DownSampleSet[i].resize(mipsCount);
			for (int j = 0; j < m_DownSampleSet[i].size(); j++)
			{
				m_DownSampleSet[i][j].Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
				m_DownSampleSet[i][j].AddImageSampler(0, { Vulture::Renderer::GetLinearSamplerHandle(), m_BloomImages[i][j].GetImageView() ,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
				);
				m_DownSampleSet[i][j].AddImageSampler(1, { Vulture::Renderer::GetLinearSamplerHandle(), m_BloomImages[i][j + 1].GetImageView(),
					VK_IMAGE_LAYOUT_GENERAL }
				);
				m_DownSampleSet[i][j].Build();
			}
		}
	}

	void Bloom::CreateBloomMips()
	{
		Image::CreateInfo info{};
		info.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		info.Width = m_InputImages[0]->GetImageSize().width;
		info.Height = m_InputImages[0]->GetImageSize().height;
		info.Aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		info.Usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		info.Tiling = VK_IMAGE_TILING_OPTIMAL;
		info.Properties = m_InputImages[0]->GetMemoryProperties();
		info.LayerCount = 1;
		info.SamplerInfo = SamplerInfo{};
		info.Type = Image::ImageType::Image2D;

		m_BloomImages.resize(m_InputImages.size());
		for (int i = 0; i < m_InputImages.size(); i++)
		{
			info.Width = m_InputImages[0]->GetImageSize().width;
			info.Height = m_InputImages[0]->GetImageSize().height;
			m_BloomImages[i].resize(10 + 1);
			// First Image is just a copy with separated bright values
			info.DebugName = "Bright Values Image";
			m_BloomImages[i][0].Init(info);

			for (int j = 0; j < 10; j++)
			{
				std::string s = "Bloom Mip Image " + std::to_string(j);
				const char* g = s.c_str();
				info.DebugName = g;
				info.Width = glm::max(1, (int)info.Width / 2);
				info.Height = glm::max(1, (int)info.Height / 2);
				m_BloomImages[i][j + 1].Init(info);
			}
		}
	}

}