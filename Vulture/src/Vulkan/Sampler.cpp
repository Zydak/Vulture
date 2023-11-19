#include "pch.h"
#include "Utility/Utility.h"

#include "Sampler.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{

	Sampler::Sampler(SamplerInfo samplerInfo)
	{
		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.magFilter = samplerInfo.FilterMode;
		samplerCreateInfo.minFilter = samplerInfo.FilterMode;
		samplerCreateInfo.mipmapMode = samplerInfo.MipmapMode;
		samplerCreateInfo.addressModeU = samplerInfo.AddressMode;
		samplerCreateInfo.addressModeV = samplerInfo.AddressMode;
		samplerCreateInfo.addressModeW = samplerInfo.AddressMode;
		samplerCreateInfo.mipLodBias = 0.0f;
		samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerCreateInfo.minLod = 0;
		samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
		samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerCreateInfo.maxAnisotropy = Device::GetDeviceProperties().limits.maxSamplerAnisotropy;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		VL_CORE_RETURN_ASSERT(vkCreateSampler(Device::GetDevice(), &samplerCreateInfo, nullptr, &m_Sampler),
			VK_SUCCESS,
			"failed to create texture sampler"
		);
	}

	Sampler::~Sampler()
	{
		vkDestroySampler(Device::GetDevice(), m_Sampler, nullptr);
	}

}