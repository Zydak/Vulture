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

		static void Init(Window& window);
		static void Destroy();
		
		static inline Swapchain& GetSwapchain() { return *s_Swapchain; }
		static inline DescriptorPool& GetDescriptorPool() { return *s_Pool; }
		static inline Sampler& GetSampler() { return *s_RendererSampler; }
		static inline VkSampler GetSamplerHandle() { return s_RendererSampler->GetSamplerHandle(); }
		static inline uint32_t& GetCurrentFrameIndex() { return s_CurrentFrameIndex; }
		static inline Mesh& GetQuadMesh() { return s_QuadMesh; }

		static bool BeginFrame();
		static bool EndFrame();

		static void RenderImGui(std::function<void()> fn);
		static void RayTrace(VkCommandBuffer cmdBuf, SBT* sbt, VkExtent2D imageSize, uint32_t depth = 1);

		static inline Image* GetEnv() { return &m_EnvMap; }

		static VkCommandBuffer GetCurrentCommandBuffer();
		static int GetFrameIndex();

		static void SaveImageToFile(const std::string& filepath, Ref<Image> image);
		static void ImGuiPass();
		static void FramebufferCopyPassImGui(DescriptorSet* descriptorWithImageSampler);
		static void FramebufferCopyPassBlit(Ref<Image> image);
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
		static void CreateDescriptorSets();
		static void WriteToFile(const char* filename, std::vector<unsigned char>& image, unsigned width, unsigned height);

		static void InitImGui();
		
		static Scope<DescriptorPool> s_Pool;
		static Window* s_Window;
		static std::vector<VkCommandBuffer> s_CommandBuffers;
		static Scope<Swapchain> s_Swapchain;

		static bool s_IsFrameStarted;
		static uint32_t s_CurrentImageIndex;
		static uint32_t s_CurrentFrameIndex;

		// TODO: delete this from here
		static Scene* s_CurrentSceneRendered;

		static bool s_IsInitialized;
	private:

		static Mesh s_QuadMesh;
		static Scope<Sampler> s_RendererSampler;

		static Ref<DescriptorSet> s_EnvToCubemapDescriptorSet;

		static Pipeline s_HDRToPresentablePipeline;

		static Image m_EnvMap;

		static Pipeline s_EnvToCubemapPipeline;

		static std::vector<Ref<Image>> s_BloomImages;

		static std::function<void()> s_ImGuiFunction;

		static VkExtent2D s_MipSize;
		static int m_MipsCount;
		static int m_PrevMipsCount;
	};
}