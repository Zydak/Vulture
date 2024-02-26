#include "pch.h"
#include "DescriptorPool.h"

namespace Vulture
{
	/**
	 * @brief Initializes the descriptor pool with the given pool sizes,
	 * maximum sets, and descriptor pool flags.
	 *
	 * @param poolSizes - Vector containing PoolSize objects representing the sizes
	 *                    of descriptor pools to be created.
	 * @param maxSets - Maximum number of descriptor sets that can be allocated from this pool.
	 * @param poolFlags - Flags specifying additional parameters of the descriptor pool.
	 */
	void DescriptorPool::Init(const std::vector<PoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags)
	{
		// Check if the descriptor pool has already been initialized.
		if (m_Initialized)
			Destroy(); // If already initialized, destroy the existing pool.

		// Destroy Existing Pools
		m_DescriptorPoolHandles.clear();
		m_DescriptorPoolHandles.resize(1);

		// Store the provided parameters.
		m_PoolSizes = poolSizes;
		m_MaxSets = maxSets;
		m_PoolFlags = poolFlags;

		// Convert the provided pool sizes to Vulkan descriptor pool sizes.
		std::vector<VkDescriptorPoolSize> poolSizesVK;
		for (int i = 0; i < poolSizes.size(); i++)
		{
			// Ensure each pool size is correctly initialized.
			VL_CORE_ASSERT(poolSizes[i], "Incorrectly initialized pool size!");

			// Convert PoolSize objects to Vulkan descriptor pool size objects.
			VkDescriptorPoolSize poolSizeVK;
			poolSizeVK.descriptorCount = poolSizes[i].DescriptorCount;
			poolSizeVK.type = poolSizes[i].PoolType;

			// Add the Vulkan descriptor pool size to the vector.
			poolSizesVK.push_back(poolSizeVK);
		}

		// Prepare the descriptor pool creation information.
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizesVK.size());
		descriptorPoolInfo.pPoolSizes = poolSizesVK.data();
		descriptorPoolInfo.maxSets = maxSets;
		descriptorPoolInfo.flags = poolFlags;

		// Attempt to create the Vulkan descriptor pool.
		VL_CORE_RETURN_ASSERT(vkCreateDescriptorPool(Device::GetDevice(), &descriptorPoolInfo, nullptr, &m_DescriptorPoolHandles[m_CurrentPool]),
			VK_SUCCESS,
			"Failed to create descriptor pool!"
		);

		// Mark the descriptor pool as initialized.
		m_Initialized = true;
	}

	/**
	 * @brief Destroys the descriptor pool.
	 */
	void DescriptorPool::Destroy()
	{
		// Iterate through each descriptor pool handle and destroy it.
		for (uint32_t i = 0; i <= m_CurrentPool; i++)
		{
			vkDestroyDescriptorPool(Device::GetDevice(), m_DescriptorPoolHandles[i], nullptr);
		}

		// Mark the descriptor pool as uninitialized.
		m_Initialized = false;
	}

	/**
	 * @brief Constructor for DescriptorPool.
	 * 
	 * @param poolSizes - Vector containing PoolSize objects representing the sizes
	 *                    of descriptor pools to be created.
	 * @param maxSets - Maximum number of descriptor sets that can be allocated from this pool.
	 * @param poolFlags - Flags specifying additional parameters of the descriptor pool.
	 */
	DescriptorPool::DescriptorPool(const std::vector<PoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags)
	{
		// Initialize the descriptor pool with the provided parameters.
		Init(poolSizes, maxSets, poolFlags);
	}

	/**
	 * @brief Destructor for DescriptorPool.
	 */
	DescriptorPool::~DescriptorPool()
	{
		// If the descriptor pool is initialized, destroy it.
		if (m_Initialized)
			Destroy();
	}

	/*
	 * @brief Attempts to allocate a descriptor set from the descriptor pool.
	 *
	 * @param descriptorSetLayout - Vulkan descriptor set layout to be used for the allocation.
	 * @param descriptor [output] - Pointer to the Vulkan descriptor set that will be allocated.
	 *
	 * @return True if the allocation is successful, false otherwise.
	 */
	bool DescriptorPool::AllocateDescriptorSets(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet* descriptor)
	{
		// Check if the descriptor pool has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Pool Not Initialized!");

		// Prepare the descriptor set allocation information.
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPoolHandles[m_CurrentPool];
		allocInfo.pSetLayouts = &descriptorSetLayout;
		allocInfo.descriptorSetCount = 1;

		// Attempt to allocate a descriptor set.
		if (vkAllocateDescriptorSets(Device::GetDevice(), &allocInfo, descriptor) != VK_SUCCESS)
		{
			// If allocation fails, create a new pool and retry allocation.
			CreateNewPool();

			// Retry allocation with the new pool.
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_DescriptorPoolHandles[m_CurrentPool];
			allocInfo.pSetLayouts = &descriptorSetLayout;
			allocInfo.descriptorSetCount = 1;

			// Attempt to allocate the descriptor set again.
			if (vkAllocateDescriptorSets(Device::GetDevice(), &allocInfo, descriptor) != VK_SUCCESS)
			{
				// If allocation fails again, return false.
				return false;
			}
		}
		// Return true indicating successful allocation.
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
		// Check if the descriptor pool has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Pool Not Initialized!");

		// Reset Pool
		vkResetDescriptorPool(Device::GetDevice(), m_DescriptorPoolHandles[index], 0);
	}

	/**
	 * @brief Creates a new Vulkan descriptor pool with the sizes and settings
	 * specified in the Init function.
	 */
	void DescriptorPool::CreateNewPool()
	{
		// Check if the descriptor pool has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Pool Not Initialized!");

		// Resize the descriptor pool handles vector to accommodate a new descriptor pool.
		m_DescriptorPoolHandles.resize(m_CurrentPool + 1);

		// Increment the current pool index.
		m_CurrentPool++;

		// Convert the pool sizes to Vulkan descriptor pool sizes.
		std::vector<VkDescriptorPoolSize> poolSizesVK;
		for (int i = 0; i < m_PoolSizes.size(); i++)
		{
			// Ensure each pool size is correctly initialized.
			VL_CORE_ASSERT(m_PoolSizes[i], "Incorrectly initialized pool size!");

			// Convert PoolSize objects to Vulkan descriptor pool size objects.
			VkDescriptorPoolSize poolSizeVK;
			poolSizeVK.descriptorCount = m_PoolSizes[i].DescriptorCount;
			poolSizeVK.type = m_PoolSizes[i].PoolType;

			// Add the Vulkan descriptor pool size to the vector.
			poolSizesVK.push_back(poolSizeVK);
		}

		// Prepare the descriptor pool creation information.
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizesVK.size());
		descriptorPoolInfo.pPoolSizes = poolSizesVK.data();
		descriptorPoolInfo.maxSets = m_MaxSets;
		descriptorPoolInfo.flags = m_PoolFlags;

		// Attempt to create the Vulkan descriptor pool.
		VL_CORE_RETURN_ASSERT(vkCreateDescriptorPool(Device::GetDevice(), &descriptorPoolInfo, nullptr, &m_DescriptorPoolHandles[m_CurrentPool]),
			VK_SUCCESS,
			"Failed to create descriptor pool!"
		);
	}

}