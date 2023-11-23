#include "pch.h"
#include "RenderPass.h"
#include "Renderer.h"

#include "Utility/Utility.h"

namespace Vulture
{

	RenderPass::RenderPass()
	{
		
	}

	RenderPass::~RenderPass()
	{
		vkDestroyRenderPass(Device::GetDevice(), m_RenderPass, nullptr);
	}

	void RenderPass::CreateRenderPass(VkRenderPassCreateInfo& renderPassInfo)
	{
		VL_CORE_RETURN_ASSERT(vkCreateRenderPass(Device::GetDevice(), &renderPassInfo, nullptr, &m_RenderPass),
			VK_SUCCESS,
			"failed to create render pass!"
		);
	}

	void RenderPass::CreatePipeline(PipelineCreateInfo& pipelineInfo)
	{
		pipelineInfo.RenderPass = m_RenderPass;
		m_Pipeline.CreatePipeline(pipelineInfo);
	}

	void RenderPass::BeginRenderPass(const std::vector<VkClearValue>& clearColors)
	{
		VL_CORE_ASSERT(m_Target != nullptr, "Target framebuffer not set!");
		VL_CORE_ASSERT(m_Target != nullptr, "Target framebuffer not set!");

		// Set up viewport
		glm::vec2 extent = m_Target->GetExtent();
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = extent.x;
		viewport.height = extent.y;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// Set up scissor
		VkRect2D scissor{
			{0, 0},
			VkExtent2D{(uint32_t)extent.x, (uint32_t)extent.y}
		};

		// Set viewport and scissor for the current command buffer
		vkCmdSetViewport(Renderer::GetCurrentCommandBuffer(), 0, 1, &viewport);
		vkCmdSetScissor( Renderer::GetCurrentCommandBuffer(), 0, 1, &scissor);

		// Set up render pass information
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_RenderPass;
		renderPassInfo.framebuffer = m_Target->GetFramebuffer();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = VkExtent2D{ (uint32_t)extent.x, (uint32_t)extent.y };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
		renderPassInfo.pClearValues = clearColors.data();

		// Begin the render pass for the current command buffer
		vkCmdBeginRenderPass(Renderer::GetCurrentCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		m_Pipeline.Bind(Renderer::GetCurrentCommandBuffer());
	}

	void RenderPass::EndRenderPass()
	{
		vkCmdEndRenderPass(Renderer::GetCurrentCommandBuffer());
	}

	void RenderPass::SetRenderTarget(Framebuffer* framebuffer)
	{
		m_Target = framebuffer;
	}

}