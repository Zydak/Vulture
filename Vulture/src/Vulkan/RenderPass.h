#pragma once
#include "pch.h"

#include <vulkan/vulkan.h>
#include "Device.h"

namespace Vulture
{

	class RenderPass
	{
	public:
		RenderPass();
		~RenderPass();

		inline VkRenderPass GetRenderPass() const { return m_RenderPass; }

		void CreateRenderPass(VkRenderPassCreateInfo renderPassInfo);

	private:
		VkRenderPass m_RenderPass;
	};

}