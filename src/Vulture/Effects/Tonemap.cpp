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

		m_ImageSize = info.OutputImage->GetImageSize();

		m_InputImage = info.InputImage;
		m_OutputImage = info.OutputImage;

		{
			Vulture::DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			Vulture::DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };

			m_Descriptor.Init(&Vulture::Renderer::GetDescriptorPool(), { bin, bin1 });
			m_Descriptor.AddImageSampler(
				0,
				{ Vulture::Renderer::GetLinearSamplerHandle(),
				info.InputImage->GetImageView(),
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }
			);
			m_Descriptor.AddImageSampler(
				1,
				{ Vulture::Renderer::GetLinearSamplerHandle(),
				info.OutputImage->GetImageView(),
				VK_IMAGE_LAYOUT_GENERAL }
			);
			m_Descriptor.Build();
		}

		// Pipeline
		{
			m_Push.Init({ VK_SHADER_STAGE_COMPUTE_BIT });

			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			std::string currentTonemapper = GetTonemapperMacroDefinition(info.Tonemapper);

			Pipeline::ComputeCreateInfo info{};
			Shader shader({ "../Vulture/src/Vulture/Shaders/Tonemap.comp" , VK_SHADER_STAGE_COMPUTE_BIT, { {currentTonemapper, ""} } });
			info.Shader = &shader;

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
		if (!m_Initialized)
			return;

		m_Pipeline.Destroy();
		
		m_Descriptor.Destroy();
		m_Push.Destroy();

		Reset();
	}

	void Tonemap::RecompileShader(Tonemappers tonemapper, bool chromaticAberration)
	{
		// Pipeline
		{
			m_Push.Init({ VK_SHADER_STAGE_COMPUTE_BIT });

			DescriptorSetLayout::Binding bin{ 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout::Binding bin1{ 1, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT };
			DescriptorSetLayout imageLayout({ bin, bin1 });

			std::string currentTonemapper = GetTonemapperMacroDefinition(tonemapper);
			std::vector<Shader::Define> defines = { {currentTonemapper, ""} };
			if (chromaticAberration)
				defines.push_back({ "USE_CHROMATIC_ABERRATION", ""});

			Pipeline::ComputeCreateInfo info{};
			Shader shader({ "../Vulture/src/Vulture/Shaders/Tonemap.comp" , VK_SHADER_STAGE_COMPUTE_BIT, defines });
			info.Shader = &shader;

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
	}

	Tonemap::Tonemap(const CreateInfo& info)
	{
		Init(info);
	}

	Tonemap::Tonemap(Tonemap&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Descriptor = std::move(other.m_Descriptor);
		m_Pipeline = std::move(other.m_Pipeline);
		m_Push = std::move(other.m_Push);
		m_ImageSize	= std::move(other.m_ImageSize);
		m_InputImage = std::move(other.m_InputImage);
		m_OutputImage = std::move(other.m_OutputImage);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();
	}

	Tonemap& Tonemap::operator=(Tonemap&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Descriptor = std::move(other.m_Descriptor);
		m_Pipeline = std::move(other.m_Pipeline);
		m_Push = std::move(other.m_Push);
		m_ImageSize = std::move(other.m_ImageSize);
		m_InputImage = std::move(other.m_InputImage);
		m_OutputImage = std::move(other.m_OutputImage);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	Tonemap::~Tonemap()
	{
		Destroy();
	}

	void Tonemap::Run(const TonemapInfo& info, VkCommandBuffer cmd)
	{
		m_OutputImage->TransitionImageLayout(
			VK_IMAGE_LAYOUT_GENERAL,
			Vulture::Renderer::GetCurrentCommandBuffer()
		);

		m_InputImage->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, Vulture::Renderer::GetCurrentCommandBuffer());

		m_Pipeline.Bind(cmd);

		m_Descriptor.Bind(
			0,
			m_Pipeline.GetPipelineLayout(),
			VK_PIPELINE_BIND_POINT_COMPUTE, 
			cmd
		);

		*(m_Push.GetDataPtr()) = info;

		m_Push.Push(m_Pipeline.GetPipelineLayout(), cmd);

		vkCmdDispatch(cmd, ((int)m_ImageSize.width) / 8 + 1, ((int)m_ImageSize.height) / 8 + 1, 1);
	}

	std::string Tonemap::GetTonemapperMacroDefinition(Tonemappers tonemapper)
	{
		switch (tonemapper)
		{
		case Vulture::Tonemap::Filmic:
			return "USE_FILMIC";
			break;
		case Vulture::Tonemap::HillAces:
			return "USE_ACES_HILL";
			break;
		case Vulture::Tonemap::NarkowiczAces:
			return "USE_ACES_NARKOWICZ";
			break;
		case Vulture::Tonemap::ExposureMapping:
			return "USE_EXPOSURE_MAPPING";
			break;
		case Vulture::Tonemap::Uncharted2:
			return "USE_UNCHARTED";
			break;
		case Vulture::Tonemap::ReinchardExtended:
			return "USE_REINHARD_EXTENDED";
			break;
		default:
			return "USE_FILMIC";
			break;
		}
	}

	void Tonemap::Reset()
	{
		m_ImageSize = { 0, 0 };
		m_InputImage = nullptr;
		m_OutputImage = nullptr;
		m_Initialized = false;
	}

}