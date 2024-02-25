#include "pch.h"
#include "DescriptorPool.h"

namespace Vulture
{
	// *************** Descriptor Pool Builder *********************

	void DescriptorPool::Init(const std::vector<PoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags)
	{
		if (m_Initialized)
			Destroy();

		m_DescriptorPoolHandles.resize(1);
		m_PoolSizes = poolSizes;
		m_MaxSets = maxSets;
		m_PoolFlags = poolFlags;
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

		VL_CORE_RETURN_ASSERT(vkCreateDescriptorPool(Device::GetDevice(), &descriptorPoolInfo, nullptr, &m_DescriptorPoolHandles[m_CurrentPool]),
			VK_SUCCESS,
			"failed to create descriptor pool!"
		);

		m_Initialized = true;
	}

	void DescriptorPool::Destroy()
	{
		for (int i = 0; i <= m_CurrentPool; i++)
		{
			vkDestroyDescriptorPool(Device::GetDevice(), m_DescriptorPoolHandles[i], nullptr);
		}
		m_Initialized = false;
	}

	// *************** Descriptor Pool *********************

	DescriptorPool::DescriptorPool(const std::vector<PoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags)
	{
		Init(poolSizes, maxSets, poolFlags);
	}

	DescriptorPool::~DescriptorPool() 
	{
		if (m_Initialized)
			Destroy();
	}

	/*
	 * @brief Allocates a Vulkan descriptor set from the descriptor pool.
	 *
	 * @param descriptorSetLayout - The Vulkan descriptor set layout to be used for the allocation.
	 * @param descriptor - Reference to the Vulkan descriptor set that will be allocated (output parameter).
	 *
	 * @return True if the allocation is successful, false otherwise.
	 */
	bool DescriptorPool::AllocateDescriptorSets(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor)
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPoolHandles[m_CurrentPool];
		allocInfo.pSetLayouts = &descriptorSetLayout;
		allocInfo.descriptorSetCount = 1;

		// TODO:
		// build a new pool whenever an old pool fills up?
		if (vkAllocateDescriptorSets(Device::GetDevice(), &allocInfo, &descriptor) != VK_SUCCESS) 
		{
			CreateNewPool();

			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_DescriptorPoolHandles[m_CurrentPool];
			allocInfo.pSetLayouts = &descriptorSetLayout;
			allocInfo.descriptorSetCount = 1;

			m_Recreated = true;

			if (vkAllocateDescriptorSets(Device::GetDevice(), &allocInfo, &descriptor) != VK_SUCCESS)
			{
				return false;
			}
		}
		return true;
	}

	/*
	 * @brief Resets the Vulkan descriptor pool, freeing all previously allocated descriptor sets.
	 *
	 * @note The function resets the descriptor pool associated with this DescriptorPool instance. After
	 * the reset, any descriptor sets that were previously allocated from this pool become invalid.
	 */
	void DescriptorPool::ResetPool(uint32_t index)
	{
		vkResetDescriptorPool(Device::GetDevice(), m_DescriptorPoolHandles[index], 0);
	}

	void DescriptorPool::CreateNewPool()
	{
		m_DescriptorPoolHandles.resize(m_CurrentPool + 1);
		m_CurrentPool++;
		std::vector<VkDescriptorPoolSize> poolSizesVK;
		for (int i = 0; i < m_PoolSizes.size(); i++)
		{
			VL_CORE_ASSERT(m_PoolSizes[i], "Incorectly initialized pool size!");

			VkDescriptorPoolSize poolSizeVK;
			poolSizeVK.descriptorCount = m_PoolSizes[i].DescriptorCount;
			poolSizeVK.type = m_PoolSizes[i].PoolType;

			poolSizesVK.push_back(poolSizeVK);
		}
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizesVK.size();
		descriptorPoolInfo.pPoolSizes = poolSizesVK.data();
		descriptorPoolInfo.maxSets = m_MaxSets;
		descriptorPoolInfo.flags = m_PoolFlags;

		VL_CORE_RETURN_ASSERT(vkCreateDescriptorPool(Device::GetDevice(), &descriptorPoolInfo, nullptr, &m_DescriptorPoolHandles[m_CurrentPool]),
			VK_SUCCESS,
			"failed to create descriptor pool!"
		);
	}

}