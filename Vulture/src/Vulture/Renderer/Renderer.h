#pragma once
#include "pch.h"
#include "../Utility/Utility.h"

#include "Vulkan/Swapchain.h"
#include "Vulkan/Descriptors.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Uniform.h"
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

		static void ImageMemoryBarrier(VkImage image, VkCommandBuffer commandBuffer, VkImageAspectFlagBits aspect,
			VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount, uint32_t baseLayer);

		static VkCommandBuffer GetCurrentCommandBuffer();
		static int GetFrameIndex();

		static void FramebufferCopyPassImGui(Ref<Uniform> uniformWithImageSampler);
		static void FramebufferCopyPassBlit(Ref<Image> image);
		static void ToneMapPass(Ref<Uniform> uniformWithImageSampler, Ref<Image> image, float exposure = 1.0f);
		static void BloomPass(Ref<Image> image, int mipsCount);
		static void EnvMapToCubemapPass(Ref<Image> envMap, Ref<Image> cubemap);

	private:
		static bool BeginFrameInternal();
		static bool EndFrameInternal();
		static void BeginRenderPass(const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, const VkRenderPass& renderPass, glm::vec2 extent);
		static void EndRenderPass();

		static void RecreateSwapchain();
		static void CreateCommandBuffers();
		static void CreatePool();
		static void CreatePipeline();
		static void CreateUniforms();
		static void CreateBloomImages(Ref<Image> image, int mipsCount);

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

		static Ref<Uniform> s_BloomSeparateBrightnessUniform;
		static Ref<Uniform> s_BloomAccumulateUniform;
		static std::vector<Ref<Uniform>> s_BloomDownSampleUniform;

		static Ref<Uniform> s_EnvToCubemapUniform;

		static Pipeline s_HDRToPresentablePipeline;
		static Pipeline s_ToneMapPipeline;
		static Pipeline s_BloomSeparateBrightnessPipeline;
		static Pipeline s_BloomAccumulatePipeline;
		static Pipeline s_BloomDownSamplePipeline;
		static Pipeline s_EnvToCubemapPipeline;

		static std::vector<Ref<Image>> s_BloomImages;

		static std::function<void()> s_ImGuiFunction;

		static glm::vec2 s_MipSize;
		static int m_PrevMipsCount;
		static int m_MipsCount;
		friend class RenderPass;


	};
}