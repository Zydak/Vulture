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

			std::vector<Image*> InputImages;
			std::vector<Image*> OutputImages;

			std::vector<Image*> AdditionalTextures;

			std::vector<Vulture::Shader::Define> Defines;

			std::string DebugName = "Post Process";
		};

		inline PushConstant<T>& GetPush() { return m_Push; }

		inline bool IsInitialized() const { return m_Initialized; }

		void Init(const CreateInfo& createInfo);

		void Destroy();

		void RecompileShader(std::vector<Shader::Define> defines);

		void Run(VkCommandBuffer cmd, uint32_t imageIndex = 0);

		Effect() = default;
		Effect(const CreateInfo& createInfo);

		Effect(const Effect& other) = delete;
		Effect& operator=(const Effect& other) = delete;
		Effect(Effect&& other) noexcept;
		Effect& operator=(Effect&& other) noexcept;

		~Effect();
	private:
		std::vector<DescriptorSet> m_Descriptors;
		Pipeline m_Pipeline;

		PushConstant<T> m_Push;
		std::string m_ShaderPath;
		std::vector<Vulture::DescriptorSetLayout::Binding> m_Bindings;

		std::vector<Image*> m_InputImages;
		std::vector<Image*> m_OutputImages;
		std::vector<Image*> m_AdditionalImages;
		VkExtent2D m_ImageSize;
		std::string m_DebugName;

		bool m_InputIsOutput = false;

		bool m_Initialized = false;

		void Reset();
	};

	template<typename T>
	Effect<T>& Vulture::Effect<T>::operator=(Effect<T>&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Pipeline			= std::move(other.m_Pipeline);
		m_Push				= std::move(other.m_Push);
		m_Descriptors		= std::move(other.m_Descriptors);
		m_ShaderPath		= std::move(other.m_ShaderPath);
		m_Bindings			= std::move(other.m_Bindings);
		m_InputImages		= std::move(other.m_InputImages);
		m_OutputImages		= std::move(other.m_OutputImages);
		m_AdditionalImages	= std::move(other.m_AdditionalImages);
		m_ImageSize			= std::move(other.m_ImageSize);
		m_DebugName			= std::move(other.m_DebugName);
		m_InputIsOutput		= std::move(other.m_InputIsOutput);
		m_Initialized		= std::move(other.m_Initialized);

		other.Reset();
	}

	template<typename T>
	Vulture::Effect<T>::Effect(Effect&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Pipeline = std::move(other.m_Pipeline);
		m_Push = std::move(other.m_Push);
		m_Descriptors = std::move(other.m_Descriptors);
		m_ShaderPath = std::move(other.m_ShaderPath);
		m_Bindings = std::move(other.m_Bindings);
		m_InputImages = std::move(other.m_InputImages);
		m_OutputImages = std::move(other.m_OutputImages);
		m_AdditionalImages = std::move(other.m_AdditionalImages);
		m_ImageSize = std::move(other.m_ImageSize);
		m_DebugName = std::move(other.m_DebugName);
		m_InputIsOutput = std::move(other.m_InputIsOutput);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	template<typename T>
	Vulture::Effect<T>::Effect(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

	template<typename T>
	void Vulture::Effect<T>::Reset()
	{
		m_Descriptors.clear();
		m_ShaderPath = "";
		m_Bindings.clear();
		m_InputImages.clear();
		m_OutputImages.clear();
		m_AdditionalImages.clear();
		m_ImageSize = { 0, 0 };
		m_DebugName = "";
		m_InputIsOutput = false;
		m_Initialized = false;
	}

	template<typename T>
	Vulture::Effect<T>::~Effect()
	{
		Destroy();
	}

	template<typename T>
	void Vulture::Effect<T>::Run(VkCommandBuffer cmd, uint32_t imageIndex /*= 0*/)
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

		if (!m_InputIsOutput)
			m_InputImages[imageIndex]->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Vulture::Renderer::GetCurrentCommandBuffer());

		m_Pipeline.Bind(cmd);

		m_Descriptors[imageIndex].Bind(
			0,
			m_Pipeline.GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_COMPUTE,
			cmd
		);

		m_Push.Push(m_Pipeline.GetPipelineLayout(), cmd);

		vkCmdDispatch(cmd, ((int)m_ImageSize.width) / 8 + 1, ((int)m_ImageSize.height) / 8 + 1, 1);
	}

	template<typename T>
	void Vulture::Effect<T>::RecompileShader(std::vector<Shader::Define> defines)
	{
		m_Push.Init({ VK_SHADER_STAGE_COMPUTE_BIT });

		DescriptorSetLayout imageLayout(m_Bindings);

		Pipeline::ComputeCreateInfo info{};
		Shader shader({ m_ShaderPath , VK_SHADER_STAGE_COMPUTE_BIT, defines });
		info.Shader = &shader;

		// Descriptor set layouts for the pipeline
		std::vector<VkDescriptorSetLayout> layouts
		{
			imageLayout.GetDescriptorSetLayoutHandle()
		};
		info.DescriptorSetLayouts = layouts;
		info.PushConstants = m_Push.GetRangePtr();
		std::string name = (m_DebugName + " Pipeline");
		info.debugName = name.c_str();

		// Create the graphics pipeline
		m_Pipeline.Init(info);
	}

	template<typename T>
	void Vulture::Effect<T>::Destroy()
	{
		if (!m_Initialized)
			return;

		m_Initialized = false;

		Reset();
	}

	template<typename T>
	void Vulture::Effect<T>::Init(const CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		m_ShaderPath = createInfo.ShaderPath;
		m_InputImages = createInfo.InputImages;
		m_OutputImages = createInfo.OutputImages;
		m_AdditionalImages = createInfo.AdditionalTextures;
		m_DebugName = createInfo.DebugName;

		m_ImageSize = createInfo.OutputImages[0]->GetImageSize();

		m_Descriptors.resize(m_OutputImages.size());
		for (int i = 0; i < m_OutputImages.size(); i++)
		{
			m_Bindings.push_back({ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT });
			m_Bindings.push_back({ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT });

			for (int j = 0; j < createInfo.AdditionalTextures.size(); j++)
			{
				m_Bindings.push_back({ j + 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT });
			}

			m_InputIsOutput = createInfo.InputImages[i]->GetImage() == createInfo.OutputImages[i]->GetImage();
			VkImageLayout inputLayout = m_InputIsOutput ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			m_Descriptors[i].Init(&Vulture::Renderer::GetDescriptorPool(), m_Bindings);
			m_Descriptors[i].AddImageSampler(
				0,
				{ Vulture::Renderer::GetLinearSamplerHandle(),
				createInfo.InputImages[i]->GetImageView(),
				inputLayout }
			);
			m_Descriptors[i].AddImageSampler(
				1,
				{ Vulture::Renderer::GetLinearSamplerHandle(),
				createInfo.OutputImages[i]->GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL }
			);

			for (int j = 0; j < createInfo.AdditionalTextures.size(); j++)
			{
				m_Descriptors[i].AddImageSampler(
					j + 2,
					{ Vulture::Renderer::GetLinearSamplerHandle(),
					createInfo.AdditionalTextures[j]->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
				);
			}

			m_Descriptors[i].Build();
		}

		// Pipeline
		{
			m_Push.Init({ VK_SHADER_STAGE_COMPUTE_BIT });

			DescriptorSetLayout imageLayout(m_Bindings);

			Pipeline::ComputeCreateInfo info{};
			Shader shader({ createInfo.ShaderPath , VK_SHADER_STAGE_COMPUTE_BIT, createInfo.Defines });
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

		m_Initialized = true;
	}

}