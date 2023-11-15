#include "pch.h"
#include "Utility/Utility.h"

#include "RenderPass.h"

namespace Vulture
{

	RenderPass::RenderPass()
	{

	}

	RenderPass::~RenderPass()
	{
		vkDestroyRenderPass(Device::GetDevice(), m_RenderPass, nullptr);
	}

	void RenderPass::CreateRenderPass(VkRenderPassCreateInfo renderPassInfo)
	{
		VL_CORE_RETURN_ASSERT(vkCreateRenderPass(Device::GetDevice(), &renderPassInfo, nullptr, &m_RenderPass),
			VK_SUCCESS,
			"failed to create render pass!"
		);
	}

}