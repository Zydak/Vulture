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

		DescriptorPool() = default;
		DescriptorPool(const std::vector<PoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags);
		~DescriptorPool();

		DescriptorPool(const DescriptorPool&) = delete;
		DescriptorPool& operator=(const DescriptorPool&) = delete;

		bool AllocateDescriptorSets(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;

		inline VkDescriptorPool GetDescriptorPoolHandle() { return m_DescriptorPoolHandle; }

		void ResetPool();

	private:
		VkDescriptorPool m_DescriptorPoolHandle;
		std::vector<PoolSize> m_PoolSizes;

		friend class DescriptorWriter;
	};

}