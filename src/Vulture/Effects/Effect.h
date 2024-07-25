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

			Image* InputImage;
			Image* OutputImage;

			std::vector<Image*> AdditionalTextures;

			std::vector<Vulture::Shader::Define> Defines;

			std::string DebugName = "Post Process";
		};

		inline PushConstant<T>& GetPush() { return m_Push; }

		inline bool IsInitialized() const { return m_Initialized; }

		void Init(const CreateInfo& createInfo);

		void Destroy();

		void RecompileShader(std::vector<Shader::Define> defines);

		void Run(VkCommandBuffer cmd);

		Effect() = default;
		Effect(const CreateInfo& createInfo);

		Effect(const Effect& other) = delete;
		Effect& operator=(const Effect& other) = delete;
		Effect(Effect&& other) noexcept;
		Effect& operator=(Effect&& other) noexcept;

		~Effect();
	private:
		DescriptorSet m_Descriptor;
		Pipeline m_Pipeline;

		PushConstant<T> m_Push;
		std::string m_ShaderPath;
		std::vector<Vulture::DescriptorSetLayout::Binding> m_Bindings;

		Image* m_InputImage;
		Image* m_OutputImage;
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
		m_Descriptor		= std::move(other.m_Descriptor);
		m_ShaderPath		= std::move(other.m_ShaderPath);
		m_Bindings			= std::move(other.m_Bindings);
		m_InputImage		= std::move(other.m_InputImage);
		m_OutputImage		= std::move(other.m_OutputImage);
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
		m_Descriptor = std::move(other.m_Descriptor);
		m_ShaderPath = std::move(other.m_ShaderPath);
		m_Bindings = std::move(other.m_Bindings);
		m_InputImage = std::move(other.m_InputImage);
		m_OutputImage = std::move(other.m_OutputImage);
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
		m_ShaderPath = "";
		m_Descriptor.Destroy();
		m_Bindings.clear();
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
	void Vulture::Effect<T>::Run(VkCommandBuffer cmd)
	{
		m_OutputImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		for (int i = 0; i < m_AdditionalImages.size(); i++)
		{
			if (m_AdditionalImages[i]->GetLayout() != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
				m_AdditionalImages[i]->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Vulture::Renderer::GetCurrentCommandBuffer());
		}

		if (!m_InputIsOutput)
			m_InputImage->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Vulture::Renderer::GetCurrentCommandBuffer());

		m_Pipeline.Bind(cmd);

		m_Descriptor.Bind(
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
		m_InputImage = createInfo.InputImage;
		m_OutputImage = createInfo.OutputImage;
		m_AdditionalImages = createInfo.AdditionalTextures;
		m_DebugName = createInfo.DebugName;

		m_ImageSize = createInfo.OutputImage->GetImageSize();

		m_Bindings.clear();
		{
			m_Bindings.push_back({ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT });
			m_Bindings.push_back({ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT });

			for (int j = 0; j < createInfo.AdditionalTextures.size(); j++)
			{
				m_Bindings.push_back({ j + 2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT });
			}

			m_InputIsOutput = createInfo.InputImage->GetImage() == createInfo.OutputImage->GetImage();
			VkImageLayout inputLayout = m_InputIsOutput ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			m_Descriptor.Init(&Vulture::Renderer::GetDescriptorPool(), m_Bindings);
			m_Descriptor.AddImageSampler(
				0,
				{ Vulture::Renderer::GetLinearSamplerHandle(),
				createInfo.InputImage->GetImageView(),
				inputLayout }
			);
			m_Descriptor.AddImageSampler(
				1,
				{ Vulture::Renderer::GetLinearSamplerHandle(),
				createInfo.OutputImage->GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL }
			);

			for (int j = 0; j < createInfo.AdditionalTextures.size(); j++)
			{
				m_Descriptor.AddImageSampler(
					j + 2,
					{ Vulture::Renderer::GetLinearSamplerHandle(),
					createInfo.AdditionalTextures[j]->GetImageView(),
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
				);
			}

			m_Descriptor.Build();
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