#pragma once
#include "pch.h"
#include "../Utility/Utility.h"

#include "Vulkan/Swapchain.h"
#include "Vulkan/Descriptors.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Uniform.h"
#include "Vulkan/RenderPass.h"
#include "Quad.h"

#include <vulkan/vulkan.h>

namespace Vulture
{
	class Scene;

	struct StorageBufferEntry
	{
		glm::mat4 ModelMatrix;
		glm::vec4 AtlasOffset; // vec2
	};

	class Renderer
	{
	public:
		~Renderer() = delete;
		Renderer() = delete;

		static void Init(Window& window);
		static void Destroy();
		static void Render(Scene& scene);
		static Scope<DescriptorPool> s_Pool;

	private:
		static bool BeginFrame();
		static void EndFrame();
		static void BeginRenderPass(const std::vector<VkClearValue>& clearColors, VkFramebuffer framebuffer, const VkRenderPass& renderPass, glm::vec2 extent);
		static void EndRenderPass();

		static void GeometryPass();
		static void PostProcessPass();

		static void UpdateStorageBuffer();

		static void RecreateSwapchain();
		static void CreateCommandBuffers();
		static void CreateUniforms();
		static void CreatePool();
		static void CreatePipeline();
		static void CreateFramebuffer();
		static void CreateRenderPass();

		static void ImageMemoryBarrier(VkImage image, VkCommandBuffer commandBuffer, VkImageAspectFlagBits aspect,
			VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t layerCount, uint32_t baseLayer);

		static VkCommandBuffer GetCurrentCommandBuffer();
		static int GetFrameIndex();

		static Window* s_Window;
		static std::vector<VkCommandBuffer> s_CommandBuffers;
		static Scope<Swapchain> s_Swapchain;

		static bool s_IsFrameStarted;
		static uint32_t s_CurrentImageIndex;
		static uint32_t s_CurrentFrameIndex;

		static Scene* s_CurrentSceneRendered;

		static bool s_IsInitialized;

	private:

		static std::vector<StorageBufferEntry> s_StorageBuffer;
		static std::vector<std::shared_ptr<Uniform>> s_ObjectsUbos;
		static std::shared_ptr<DescriptorSetLayout> s_AtlasSetLayout;
		static Quad s_QuadMesh;
		static Scope<Sampler> s_RendererSampler;

		static Pipeline s_GeometryPipeline;
		static Pipeline s_HDRToPresentablePipeline;

		static RenderPass s_HDRRenderPass;
		static std::vector<Scope<Framebuffer>> s_HDRFramebuffer;
		static std::vector<Ref<Uniform>> s_HDRUniforms;
	};
}