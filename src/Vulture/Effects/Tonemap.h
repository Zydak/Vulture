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
	class Tonemap
	{
	public:

		enum Tonemappers
		{
			Filmic,
			HillAces,
			NarkowiczAces,
			ExposureMapping,
			Uncharted2,
			ReinchardExtended
		};

		struct CreateInfo
		{
			Image* InputImage = nullptr;
			Image* OutputImage = nullptr;
		};

		struct TonemapInfo
		{
			float Contrast = 1.0f;
			float Saturation = 1.0f;
			float Exposure = 1.0f;
			float Brightness = 0.0f;
			float Vignette = 0.0f;
			float Gamma = 1.0f;
			float Temperature = 0.0f;
			float Tint = 0.0f;
			glm::vec4 ColorFilter = { 1.0f, 1.0f, 1.0f, 1.0f };

			glm::vec2 AberrationOffsets[3] = { {2.0f, -2.0f}, {-2.0f, 2.0f}, {2.0f, -2.0f} };
			float AberrationVignette = 1.0f;

			float whitePointReinhard = 3.0f;

			Tonemappers Tonemapper = Tonemappers::Filmic;
			bool ChromaticAberration = false;
		};

		void Init(const CreateInfo& info);
		void Destroy();

		inline bool IsInitialized() const { return m_Initialized; }

		Tonemap() = default;
		Tonemap(const CreateInfo& info);
		~Tonemap();

		Tonemap(const Tonemap& other) = delete;
		Tonemap& operator=(const Tonemap& other) = delete;
		Tonemap(Tonemap&& other) noexcept;
		Tonemap& operator=(Tonemap&& other) noexcept;

		void Run(const TonemapInfo& info, VkCommandBuffer cmd);
	private:
		std::string GetTonemapperMacroDefinition(Tonemappers tonemapper);

		void RecompileShader(Tonemappers tonemapper, bool chromaticAberration);

		DescriptorSet m_Descriptor;
		Pipeline m_Pipeline;
		PushConstant<TonemapInfo> m_Push;

		VkExtent2D m_ImageSize = { 0, 0 };

		Tonemappers m_CurrentTonemapper = Tonemappers::Filmic;
		bool m_EnableChromaticAberration = false;

		Image* m_InputImage = nullptr;
		Image* m_OutputImage = nullptr;

		bool m_Initialized = false;

		void Reset();
	};
}