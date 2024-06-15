#pragma once

#include "Vulkan/Device.h"

namespace Vulture
{
	class DescriptorPool
	{
	public:
		struct PoolSize
		{
			VkDescriptorType PoolType = VkDescriptorType::VK_DESCRIPTOR_TYPE_MAX_ENUM;
			uint32_t DescriptorCount = -1;

			operator bool() const
			{
				return (PoolType != VK_DESCRIPTOR_TYPE_MAX_ENUM) && (DescriptorCount != -1);
			}
		};
		
		void Init(const std::vector<PoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags);
		void Destroy();

		DescriptorPool() = default;
		DescriptorPool(const std::vector<PoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags);
		~DescriptorPool();

		DescriptorPool(const DescriptorPool&) = delete;
		DescriptorPool& operator=(const DescriptorPool&) = delete;

		DescriptorPool(DescriptorPool&&) noexcept;
		DescriptorPool& operator=(DescriptorPool&&) noexcept;

		bool AllocateDescriptorSets(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet* descriptor);

		inline VkDescriptorPool GetDescriptorPoolHandle(uint32_t index = 0) { return m_DescriptorPoolHandles[index]; }

		inline bool IsInitialized() const { return m_Initialized; }

		void ResetPool(uint32_t index = 0);

	private:
		void CreateNewPool();

		uint32_t m_CurrentPool = 0;
		std::vector<VkDescriptorPool> m_DescriptorPoolHandles;
		std::vector<PoolSize> m_PoolSizes;
		uint32_t m_MaxSets;
		VkDescriptorPoolCreateFlags m_PoolFlags;

		bool m_Initialized = false;

		void Reset();

		friend class DescriptorWriter;
	};

}