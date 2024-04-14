#include "pch.h"
#include "Utility/Utility.h"

#include "Sampler.h"

#include <vulkan/vulkan_core.h>

namespace Vulture
{

	void Sampler::Init(const SamplerInfo& samplerInfo)
	{
		if (m_Initialized)
			Destroy();

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
		samplerCreateInfo.maxAnisotropy = Device::GetDeviceProperties().properties.limits.maxSamplerAnisotropy;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
		samplerCreateInfo.compareEnable = VK_FALSE;
		VL_CORE_RETURN_ASSERT(vkCreateSampler(Device::GetDevice(), &samplerCreateInfo, nullptr, &m_SamplerHandle),
			VK_SUCCESS,
			"failed to create texture sampler"
		);

		m_Initialized = true;
	}

	void Sampler::Destroy()
	{
		vkDestroySampler(Device::GetDevice(), m_SamplerHandle, nullptr);
		m_Initialized = false;
	}

	Sampler::Sampler(const SamplerInfo& samplerInfo)
	{
		Init(samplerInfo);
	}

	Sampler::~Sampler()
	{
		if (m_Initialized)
			Destroy();
	}

}