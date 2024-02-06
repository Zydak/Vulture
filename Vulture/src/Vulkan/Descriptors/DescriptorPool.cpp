#include "pch.h"
#include "DescriptorPool.h"

namespace Vulture
{
	// *************** Descriptor Pool Builder *********************

	void DescriptorPool::Init(const std::vector<PoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags)
	{
		m_PoolSizes = poolSizes;
		std::vector<VkDescriptorPoolSize> poolSizesVK;
		for (int i = 0; i < poolSizes.size(); i++)
		{
			VL_CORE_ASSERT(poolSizes[i], "Incorectly initialized pool size!");
			
			VkDescriptorPoolSize poolSizeVK;
			poolSizeVK.descriptorCount = poolSizes[i].DescriptorCount;
			poolSizeVK.type = poolSizes[i].PoolType;

			poolSizesVK.push_back(poolSizeVK);
		}
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizesVK.size();
		descriptorPoolInfo.pPoolSizes = poolSizesVK.data();
		descriptorPoolInfo.maxSets = maxSets;
		descriptorPoolInfo.flags = poolFlags;

		VL_CORE_RETURN_ASSERT(vkCreateDescriptorPool(Device::GetDevice(), &descriptorPoolInfo, nullptr, &m_DescriptorPoolHandle),
			VK_SUCCESS,
			"failed to create descriptor pool!"
		);
	}

	// *************** Descriptor Pool *********************

	DescriptorPool::DescriptorPool(const std::vector<PoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags)
	{
		Init(poolSizes, maxSets, poolFlags);
	}

	DescriptorPool::~DescriptorPool() 
	{
		vkDestroyDescriptorPool(Device::GetDevice(), m_DescriptorPoolHandle, nullptr);
	}

	/*
	 * @brief Allocates a Vulkan descriptor set from the descriptor pool.
	 *
	 * @param descriptorSetLayout - The Vulkan descriptor set layout to be used for the allocation.
	 * @param descriptor - Reference to the Vulkan descriptor set that will be allocated (output parameter).
	 *
	 * @return True if the allocation is successful, false otherwise.
	 */
	bool DescriptorPool::AllocateDescriptorSets(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPoolHandle;
		allocInfo.pSetLayouts = &descriptorSetLayout;
		allocInfo.descriptorSetCount = 1;

		// TODO:
		// build a new pool whenever an old pool fills up?
		if (vkAllocateDescriptorSets(Device::GetDevice(), &allocInfo, &descriptor) != VK_SUCCESS) 
		{
			return false;
		}
		return true;
	}

	/*
	 * @brief Resets the Vulkan descriptor pool, freeing all previously allocated descriptor sets.
	 *
	 * @note The function resets the descriptor pool associated with this DescriptorPool instance. After
	 * the reset, any descriptor sets that were previously allocated from this pool become invalid.
	 */
	void DescriptorPool::ResetPool()
	{
		vkResetDescriptorPool(Device::GetDevice(), m_DescriptorPoolHandle, 0);
	}
}