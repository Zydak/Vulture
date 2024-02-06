#pragma once
#include "pch.h"
#include "Vulkan/Pipeline.h"
#include "Vulkan/Device.h"
#include "Vulkan/Framebuffer.h"

#include <vulkan/vulkan.h>

namespace Vulture
{

	struct RenderPassInfo
	{
		VkRenderPassCreateInfo renderPassInfo;
		Pipeline::CreateInfo pipelineInfo;
	};

	class RenderPass
	{
	public:
		RenderPass();
		~RenderPass();

		void CreateRenderPass(VkRenderPassCreateInfo& renderPassInfo);
		void CreatePipeline(Pipeline::CreateInfo& pipelineInfo);

		void BeginRenderPass(const std::vector<VkClearValue>& clearColors);
		void EndRenderPass();
		void SetRenderTarget(Framebuffer* framebuffer);

		inline VkRenderPass GetRenderPass() const { return m_RenderPass; }
		inline const Pipeline& GetPipeline() const { return m_Pipeline; }

	private:

		VkRenderPass m_RenderPass;
		Pipeline m_Pipeline{};
		Framebuffer* m_Target = nullptr;
	};
}