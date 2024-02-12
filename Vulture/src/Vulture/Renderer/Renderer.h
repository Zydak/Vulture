#pragma once
#include "pch.h"
#include "../Utility/Utility.h"

#include "Vulkan/Swapchain.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/PushConstant.h"
#include "Vulkan/SBT.h"
#include "Mesh.h"
#include "RenderPass.h"

#include <vulkan/vulkan.h>

#ifndef VL_IMGUI
#define VL_IMGUI
#endif

namespace Vulture
{
	class Scene;

	class Renderer
	{
	public:
		~Renderer() = delete;
		Renderer() = delete;

		static void Init(Window& window);
		static void Destroy();
		
		static inline Swapchain& GetSwapchain() { return *s_Swapchain; }
		static inline DescriptorPool& GetDescriptorPool() { return *s_Pool; }
		static inline Sampler& GetSampler() { return *s_RendererSampler; }
		static inline uint32_t& GetCurrentFrameIndex() { return s_CurrentFrameIndex; }
		static inline Mesh& GetQuadMesh() { return s_QuadMesh; }

		static bool BeginFrame();
		static bool EndFrame();

		static void RenderImGui(std::function<void()> fn);
		static void RayTrace(VkCommandBuffer cmdBuf, SBT* sbt, VkExtent2D imageSize, uint32_t depth = 1);

		static inline Image* GetEnv() { return &m_EnvMap; }

		static VkCommandBuffer GetCurrentCommandBuffer();
		static int GetFrameIndex();

		static void SaveImageToFile(const std::string& filepath, Ref<Image> image, VkCommandBuffer cmd);
		static void ImGuiPass();
		static void FramebufferCopyPassImGui(Ref<DescriptorSet> descriptorWithImageSampler);
		static void FramebufferCopyPassBlit(Ref<Image> image);
		static void EnvMapToCubemapPass(Ref<Image> envMap, Ref<Image> cubemap);
		static void SampleEnvMap(Image* image);

		struct BloomInfo
		{
			float Threshold = 1.5f;
			float Strength = 0.5f;
			int MipCount = 6;
		};
		static void BloomPass(Ref<Image> inputImage, Ref<Image> outputImage, BloomInfo bloomInfo = BloomInfo());
		
		struct TonemapInfo
		{
			float Contrast = 1.0f;
			float Saturation = 1.0f;
			float Exposure = 1.0f;
			float Brightness = 1.0f;
			float Vignette = 0.0f;
		};
		static void ToneMapPass(Ref<DescriptorSet> tonemapSet, VkExtent2D dstSize, TonemapInfo tonInfo = TonemapInfo());
	private:
		static bool BeginFrameInternal();
		static bool EndFrameInternal();
		static void BeginRenderPass(const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, const VkRenderPass& renderPass, glm::vec2 extent);
		static void EndRenderPass();

		static void RecreateSwapchain();
		static void CreateCommandBuffers();
		static void CreatePool();
		static void CreatePipeline();
		static void CreateDescriptorSets();
		static void CreateBloomImages(Ref<Image> inputImage, Ref<Image> outputImage, int mipsCount);
		static void CreateBloomDescriptors(Ref<Image> inputImage, Ref<Image> outputImage, int mipsCount);
		static void WriteToFile(const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height);
		
		static Scope<DescriptorPool> s_Pool;
		static Window* s_Window;
		static std::vector<VkCommandBuffer> s_CommandBuffers;
		static Scope<Swapchain> s_Swapchain;

		static bool s_IsFrameStarted;
		static uint32_t s_CurrentImageIndex;
		static uint32_t s_CurrentFrameIndex;

		static Scene* s_CurrentSceneRendered;

		static bool s_IsInitialized;
	private:

		static Mesh s_QuadMesh;
		static Scope<Sampler> s_RendererSampler;

		static PushConstant<TonemapInfo> s_TonemapperPush;
		static PushConstant<BloomInfo> s_BloomPush;

		static std::array<Ref<DescriptorSet>, Swapchain::MAX_FRAMES_IN_FLIGHT> s_BloomSeparateBrightnessDescriptorSet;
		static std::array<std::vector<Ref<DescriptorSet>>, Swapchain::MAX_FRAMES_IN_FLIGHT> s_BloomAccumulateDescriptorSet;
		static std::array<std::vector<Ref<DescriptorSet>>, Swapchain::MAX_FRAMES_IN_FLIGHT> s_BloomDownSampleDescriptorSet;

		static Ref<DescriptorSet> s_EnvToCubemapDescriptorSet;

		static Pipeline s_HDRToPresentablePipeline;
		static Pipeline s_ToneMapPipeline;
		static std::array<Pipeline, Swapchain::MAX_FRAMES_IN_FLIGHT> s_BloomSeparateBrightnessPipeline;
		static std::array<Pipeline, Swapchain::MAX_FRAMES_IN_FLIGHT> s_BloomAccumulatePipeline;
		static std::array<Pipeline, Swapchain::MAX_FRAMES_IN_FLIGHT> s_BloomDownSamplePipeline;

		static Image m_EnvMap;

		static Pipeline s_EnvToCubemapPipeline;

		static std::vector<Ref<Image>> s_BloomImages;

		static std::function<void()> s_ImGuiFunction;

		static VkExtent2D s_MipSize;
		static int m_MipsCount;
		static int m_PrevMipsCount;
	};
}