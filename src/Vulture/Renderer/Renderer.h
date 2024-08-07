// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "pch.h"
#include "../Utility/Utility.h"

#include "Vulkan/Swapchain.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/DescriptorSet.h"
#include "Vulkan/PushConstant.h"
#include "Vulkan/SBT.h"
#include "Mesh.h"

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

		static void Init(Window& window, uint32_t maxFramesInFlight);
		static void Destroy();
		
		static inline Swapchain& GetSwapchain() { return *s_Swapchain; }
		static inline DescriptorPool& GetDescriptorPool() { return *s_Pool; }
		static inline Sampler& GetLinearSampler() { return s_RendererLinearSampler; }
		static inline Sampler& GetLinearRepeatSampler() { return s_RendererLinearSamplerRepeat; }
		static inline Sampler& GetNearestSampler() { return s_RendererNearestSampler; }
		static inline uint32_t& GetCurrentFrameIndex() { return s_CurrentFrameIndex; }
		static inline Mesh& GetQuadMesh() { return s_QuadMesh; }
		static inline float GetAspectRatio() { return s_Swapchain->GetExtentAspectRatio(); }
		static inline VkExtent2D GetExtent() { return s_Swapchain->GetSwapchainExtent(); }

		static bool BeginFrame();
		static bool EndFrame();

		static void SetImGuiFunction(std::function<void()> fn);
		static void RayTrace(VkCommandBuffer cmdBuf, SBT* sbt, VkExtent2D imageSize, uint32_t depth = 1);

		static VkCommandBuffer GetCurrentCommandBuffer();
		static int GetFrameIndex();
		inline static int GetImageIndex() { return s_CurrentImageIndex; };

		static void SaveImageToFile(const std::string& filepath, Image* image);
		static void ImGuiPass();
		static void FramebufferCopyPassImGui(DescriptorSet* descriptorWithImageSampler);
		static void FramebufferCopyPassBlit(Ref<Image> image);
		static void EnvMapToCubemapPass(Ref<Image> envMap, Ref<Image> cubemap);

		static void BeginRenderPass(const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, VkRenderPass renderPass, VkExtent2D extent);
		static void EndRenderPass();

		static inline uint32_t GetMaxFramesInFlight() { return m_MaxFramesInFlight; }

		static inline bool IsInitialized() { return s_Initialized; }

	private:
		static bool BeginFrameInternal();
		static bool EndFrameInternal();

		static void RecreateSwapchain();
		static void CreateCommandBuffers();
		static void CreatePool();
		static void CreatePipeline();
		static void CreateDescriptorSets();
		static void WriteToFile(const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height);

		static void InitImGui();
		static void DestroyImGui();

		static uint32_t m_MaxFramesInFlight;
		
		static Scope<DescriptorPool> s_Pool;
		static Window* s_Window;
		static std::vector<VkCommandBuffer> s_CommandBuffers;
		static Scope<Swapchain> s_Swapchain;

		static bool s_IsFrameStarted;
		static uint32_t s_CurrentImageIndex;
		static uint32_t s_CurrentFrameIndex;

		// TODO: delete this from here
		static Scene* s_CurrentSceneRendered;

		static Mesh s_QuadMesh;
		static Sampler s_RendererLinearSampler;
		static Sampler s_RendererLinearSamplerRepeat;
		static Sampler s_RendererNearestSampler;

		static Ref<DescriptorSet> s_EnvToCubemapDescriptorSet;

		static Pipeline s_HDRToPresentablePipeline;
		static Pipeline s_EnvToCubemapPipeline;

		static std::function<void()> s_ImGuiFunction;

		static bool s_Initialized;
	};
}