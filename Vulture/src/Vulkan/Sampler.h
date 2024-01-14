#pragma once
#include "pch.h"

#include "Device.h"

namespace Vulture
{

	struct SamplerInfo
	{
		VkSamplerAddressMode AddressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		VkFilter FilterMode = VK_FILTER_LINEAR;
		VkSamplerMipmapMode MipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	};

	class Sampler
	{
	public:
		Sampler(SamplerInfo samplerInfo);
		~Sampler();

		inline VkSampler GetSampler() const { return m_Sampler; }

	private:
		VkSampler m_Sampler;
	};

}