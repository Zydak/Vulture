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

		Sampler(const Sampler& other) = delete;
		Sampler& operator=(const Sampler& other) = delete;
		Sampler(Sampler&& other) noexcept;
		Sampler& operator=(Sampler&& other) noexcept;

		inline VkSampler GetSamplerHandle() const { return m_SamplerHandle; }

		inline bool IsInitialized() const { return m_Initialized; }

	private:
		VkSampler m_SamplerHandle = VK_NULL_HANDLE;
		bool m_Initialized = false;

		void Reset();
	};

}