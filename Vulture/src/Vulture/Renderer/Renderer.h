#pragma once
#include "pch.h"
#include "../Utility/Utility.h"

#include "Vulkan/Swapchain.h"
#include "Vulkan/Descriptors.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Uniform.h"
#include "Quad.h"
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
		static inline Quad& GetQuadMesh() { return s_QuadMesh; }

		static bool BeginFrame();
		static bool EndFrame();

#ifdef VL_IMGUI
		static void RenderImGui(std::function<void()> fn);
#endif

		static void ImageMemoryBarrier(VkImage image, VkCommandBuffer commandBuffer, VkImageAspectFlagBits aspect,
			VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount, uint32_t baseLayer);

		static VkCommandBuffer GetCurrentCommandBuffer();
		static int GetFrameIndex();

		static void FramebufferCopyPass(Uniform* uniformWithImageSampler);

	private:
		static bool BeginFrameInternal();
		static bool EndFrameInternal();
		static void BeginRenderPass(const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, const VkRenderPass& renderPass, glm::vec2 extent);
		static void EndRenderPass();

		static void RecreateSwapchain();
		static void CreateCommandBuffers();
		static void CreatePool();
		static void CreatePipeline();

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

		static Quad s_QuadMesh;
		static Scope<Sampler> s_RendererSampler;

		static Pipeline s_HDRToPresentablePipeline;

#ifdef VL_IMGUI
		static std::function<void()> s_ImGuiFunction;
#endif

		friend class RenderPass;
	};
}