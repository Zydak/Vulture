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
		void Init(const SamplerInfo& samplerInfo);
		void Destroy();

		Sampler() = default;
		Sampler(const SamplerInfo& samplerInfo);
		~Sampler();

		inline VkSampler GetLinearSamplerHandle() const { return m_SamplerHandle; }

	private:
		VkSampler m_SamplerHandle;
		bool m_Initialized = false;
	};

}