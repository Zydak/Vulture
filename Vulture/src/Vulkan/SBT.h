#pragma once

#include "Device.h"
#include "Pipeline.h"

namespace Vulture
{
	class SBT
	{
	private:
		struct Info;
	public:
		struct CreateInfo
		{
			uint32_t RGenCount = 0xFFFFFFFF;
			uint32_t MissCount = 0xFFFFFFFF;
			uint32_t HitCount = 0xFFFFFFFF;
			uint32_t CallableCount = 0xFFFFFFFF;

			Pipeline* RayTracingPipeline = nullptr;

			operator bool() const
			{
				return (RGenCount != 0xFFFFFFFF) && (MissCount != 0xFFFFFFFF) && (HitCount != 0xFFFFFFFF) && (CallableCount != 0xFFFFFFFF) && (RayTracingPipeline != nullptr);
			}
		};

		void Init(CreateInfo* createInfo);
		void Destroy();

		SBT(CreateInfo* createInfo);

		inline Info GetInfo() const { return m_Info; }

		inline uint32_t GetRGenCount() const { return m_Info.RGenCount; }
		inline uint32_t GetHitCount() const { return m_Info.HitCount; }
		inline uint32_t GetMissCount() const { return m_Info.MissCount; }
		inline uint32_t GetCallCount() const { return m_Info.CallableCount; }

		inline VkStridedDeviceAddressRegionKHR GetRGenRegion() const { return m_Info.RgenRegion; }
		inline VkStridedDeviceAddressRegionKHR GetMissRegion() const { return m_Info.MissRegion; }
		inline VkStridedDeviceAddressRegionKHR GetHitRegion() const { return m_Info.HitRegion; }
		inline VkStridedDeviceAddressRegionKHR GetCallRegion() const { return m_Info.CallRegion; }

		SBT() = default;
		~SBT();
		SBT(const SBT&) = delete;
		SBT& operator=(const SBT&) = delete;


	private:
		struct Info
		{
			uint32_t RGenCount;
			uint32_t MissCount;
			uint32_t HitCount;
			uint32_t CallableCount;
			Pipeline* RayTracingPipeline;

			VkStridedDeviceAddressRegionKHR RgenRegion{};
			VkStridedDeviceAddressRegionKHR MissRegion{};
			VkStridedDeviceAddressRegionKHR HitRegion{};
			VkStridedDeviceAddressRegionKHR CallRegion{};
			Vulture::Buffer RtSBTBuffer;

			bool Initialized = false;
		};

		Info m_Info;
	};
}