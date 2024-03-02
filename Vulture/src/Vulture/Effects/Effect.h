#pragma once

#include "Vulkan/Device.h"
#include "Vulkan/Image.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/PushConstant.h"
#include "Vulkan/Pipeline.h"

namespace Vulture
{
	template<typename T>
	class Effect
	{
	public:
		struct CreateInfo
		{
			std::string ShaderPath;

			std::vector<Ref<Image>> InputImages;
			std::vector<Ref<Image>> OutputImages;

			std::vector<Ref<Image>> AdditionalTextures;

			std::string DebugName = "Post Process";
		};

		void Init(const CreateInfo& createInfo)
		{
			if (m_Initialized)
				Destroy();

			m_InputImages = createInfo.InputImages;
			m_OutputImages = createInfo.OutputImages;
			m_AdditionalImages = createInfo.AdditionalTextures;

			m_ImageSize = createInfo.OutputImages[0]->GetImageSize();

			m_Descriptor.resize(m_OutputImages.size());
			std::vector<Vulture::DescriptorSetLayout::Binding> bindings;
			for (int i = 0; i < m_OutputImages.size(); i++)
			{
				bindings.push_back({ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT });
				bindings.push_back({ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT });

				for (int i = 0; i < createInfo.AdditionalTextures.size(); i++)
				{
					bindings.push_back({ (uint32_t)i + 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT });
				}

				m_Descriptor[i].Init(&Vulture::Renderer::GetDescriptorPool(), bindings);
				m_Descriptor[i].AddImageSampler(
					0,
					Vulture::Renderer::GetSamplerHandle(),
					createInfo.InputImages[i]->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				);
				m_Descriptor[i].AddImageSampler(
					1,
					Vulture::Renderer::GetSamplerHandle(),
					createInfo.OutputImages[i]->GetImageView(),
					VK_IMAGE_LAYOUT_GENERAL
				);

				for (int j = 0; j < createInfo.AdditionalTextures.size(); j++)
				{
					m_Descriptor[i].AddImageSampler(
						j + 2,
						Vulture::Renderer::GetSamplerHandle(),
						createInfo.AdditionalTextures[j]->GetImageView(),
						VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					);
				}

				m_Descriptor[i].Build();
			}

			// Pipeline
			{
				m_Push.Init({ VK_SHADER_STAGE_COMPUTE_BIT });

				DescriptorSetLayout imageLayout(bindings);

				Pipeline::ComputeCreateInfo info{};
				Shader shader({ createInfo.ShaderPath , VK_SHADER_STAGE_COMPUTE_BIT });
				info.Shader = &shader;

				// Descriptor set layouts for the pipeline
				std::vector<VkDescriptorSetLayout> layouts
				{
					imageLayout.GetDescriptorSetLayoutHandle()
				};
				info.DescriptorSetLayouts = layouts;
				info.PushConstants = m_Push.GetRangePtr();
				std::string name = (createInfo.DebugName + " Pipeline");
				info.debugName = name.c_str();

				// Create the graphics pipeline
				m_Pipeline.Init(info);
			}
		}

		void Destroy()
		{
			m_Initialized = false;
		}

		void Run(T push, VkCommandBuffer cmd, uint32_t imageIndex = 0)
		{
			m_OutputImages[imageIndex]->TransitionImageLayout(
				VK_IMAGE_LAYOUT_GENERAL,
				Vulture::Renderer::GetCurrentCommandBuffer()
			);

			for (int i = 0; i < m_AdditionalImages.size(); i++)
			{
				if (m_AdditionalImages[i]->GetLayout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
					m_AdditionalImages[i]->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Vulture::Renderer::GetCurrentCommandBuffer());
			}

			m_InputImages[imageIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Vulture::Renderer::GetCurrentCommandBuffer());

			m_Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

			m_Descriptor[imageIndex].Bind(
				0,
				m_Pipeline.GetPipelineLayout(),
				VK_PIPELINE_BIND_POINT_COMPUTE,
				cmd
			);

			*(m_Push.GetDataPtr()) = push;

			m_Push.Push(m_Pipeline.GetPipelineLayout(), cmd);

			vkCmdDispatch(cmd, ((int)m_ImageSize.width) / 8 + 1, ((int)m_ImageSize.height) / 8 + 1, 1);
		}

		Effect() = default;
		Effect(const Effect&) = delete;

		~Effect()
		{
			if (m_Initialized)
				Destroy();
		}
	private:
		std::vector<DescriptorSet> m_Descriptor;
		Pipeline m_Pipeline;

		PushConstant<T> m_Push;

		std::vector<Ref<Image>> m_InputImages;
		std::vector<Ref<Image>> m_OutputImages;
		std::vector<Ref<Image>> m_AdditionalImages;
		VkExtent2D m_ImageSize;

		bool m_Initialized = false;
	};
}