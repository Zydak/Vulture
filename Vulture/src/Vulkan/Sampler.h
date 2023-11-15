#pragma once
#include "pch.h"

#include "Device.h"

namespace Vulture
{

	struct SamplerInfo
	{
		VkSamplerAddressMode AddressMode;
		VkFilter FilterMode;
		VkSamplerMipmapMode MipmapMode;
	};

	class Sampler
	{
	public:
		Sampler(SamplerInfo samplerInfo, uint32_t mipLevels = 0);
		~Sampler();

		inline VkSampler GetSampler() { return m_Sampler; }

	private:
		VkSampler m_Sampler;
	};

}