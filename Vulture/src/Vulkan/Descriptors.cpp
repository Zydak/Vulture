#include "pch.h"
#include "Utility/Utility.h"

#include "Descriptors.h"

namespace Vulture
{

	// *************** Descriptor Set Layout Builder *********************

	/*
	 * @brief Adds a new binding to the descriptor set layout builder.
	 *
	 * @note Ensure that the binding is not already in use.
	 * 
	 * @param binding - The binding index to associate with the descriptor.
	 * @param descriptorType - The type of the descriptor in the binding.
	 * @param stageFlags - The shader stages in which this binding will be accessible.
	 * @param count (Optional) - The number of descriptors in the binding.
	 */
	void DescriptorSetLayout::Builder::AddBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count)
	{
		VL_CORE_ASSERT(m_Bindings.count(binding) == 0, "Binding already in use");
		VkDescriptorSetLayoutBinding layoutBinding{};
		layoutBinding.binding = binding;
		layoutBinding.descriptorType = descriptorType;
		layoutBinding.descriptorCount = count;
		layoutBinding.stageFlags = stageFlags;
		m_Bindings[binding] = layoutBinding;
	}

	// *************** Descriptor Set Layout *********************

	DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout::Builder builder) : m_Bindings(builder.m_Bindings)
	{
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
		for (auto& kv : builder.m_Bindings)
		{ 
			setLayoutBindings.push_back(kv.second); 
		}

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
		descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
		descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

		VL_CORE_RETURN_ASSERT(vkCreateDescriptorSetLayout(Device::GetDevice(), &descriptorSetLayoutInfo, nullptr, &m_DescriptorSetLayout), 
			VK_SUCCESS,
			"failed to create descriptor set layout!"
		);
	}

	DescriptorSetLayout::~DescriptorSetLayout() 
	{ 
		vkDestroyDescriptorSetLayout(Device::GetDevice(), m_DescriptorSetLayout, nullptr); 
	}

	// *************** Descriptor Pool Builder *********************

	/*
	 * @brief Adds a descriptor pool size to the descriptor pool builder.
	 *
	 * @param descriptorType - The Vulkan descriptor type for which the pool size is being specified.
	 * @param count - The number of descriptors of the specified type to allocate in the descriptor pool.
	 */
	void DescriptorPool::Builder::AddPoolSize(VkDescriptorType descriptorType, uint32_t count)
	{
		m_PoolSizes.push_back({ descriptorType, count });
		if (m_PoolSize < count) { m_PoolSize = count; }
	}

	/*
	 * @brief This function sets the creation flags for the descriptor pool being built, allowing customization
	 * of its behavior during creation. The flags parameter is a combination of VkDescriptorPoolCreateFlagBits.
	 *
	 * @param flags - The descriptor pool creation flags to be set.
	 *
	 * @note Use this function to specify additional behaviors for the descriptor pool, such as supporting
	 * free descriptor sets or resetting individual descriptor sets.
	 */
	void DescriptorPool::Builder::SetPoolFlags(VkDescriptorPoolCreateFlags flags)
	{
		m_PoolFlags = flags;
	}

	/*
	 * @brief This function specifies the maximum number of descriptor sets that the descriptor pool can allocate.
	 *
	 * @param count - The maximum number of descriptor sets the descriptor pool can allocate.
	 */
	void DescriptorPool::Builder::SetMaxSets(uint32_t count)
	{
		m_MaxSets = count;
	}

	// *************** Descriptor Pool *********************

	DescriptorPool::DescriptorPool(const DescriptorPool::Builder& builder)
	{
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(builder.m_PoolSizes.size());
		descriptorPoolInfo.pPoolSizes = builder.m_PoolSizes.data();
		descriptorPoolInfo.maxSets = builder.m_MaxSets;
		descriptorPoolInfo.flags = builder.m_PoolFlags;

		VL_CORE_RETURN_ASSERT(vkCreateDescriptorPool(Device::GetDevice(), &descriptorPoolInfo, nullptr, &m_DescriptorPool),
			VK_SUCCESS,
			"failed to create descriptor pool!"
		);
	}

	DescriptorPool::~DescriptorPool() { vkDestroyDescriptorPool(Device::GetDevice(), m_DescriptorPool, nullptr); }

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
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.pSetLayouts = &descriptorSetLayout;
		allocInfo.descriptorSetCount = 1;

		// TODO:
		// Might want to create a "DescriptorPoolManager" class that handles this case, and builds
		// a new pool whenever an old pool fills up.
		if (vkAllocateDescriptorSets(Device::GetDevice(), &allocInfo, &descriptor) != VK_SUCCESS) { return false; }
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
		vkResetDescriptorPool(Device::GetDevice(), m_DescriptorPool, 0); 
	}

	// *************** Descriptor Writer *********************

	DescriptorWriter::DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool) : m_SetLayout{ setLayout }, m_Pool{ pool } {}

	/*
	 * @brief Writes buffer descriptor information to a specified binding in a Vulkan descriptor set.
	 *
	 * @param binding - The binding index to which the buffer descriptor information is to be written.
	 * @param bufferInfo - Pointer to the VkDescriptorBufferInfo structure containing buffer-specific details.
	 *
	 * @note The 'bufferInfo' parameter must remain valid until the DescriptorWriter::Build function is called.
	 */
	void DescriptorWriter::WriteBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo)
	{
		VL_CORE_ASSERT(m_SetLayout.m_Bindings.count(binding) == 1, "Layout does not contain specified binding : {0}", binding);

		auto& bindingDescription = m_SetLayout.m_Bindings[binding];

		VL_CORE_ASSERT(bindingDescription.descriptorCount == 1, "Writing multiple descriptors is not supported for now. descriptorCount = {0}", bindingDescription.descriptorCount);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.descriptorType;
		write.dstBinding = binding;
		write.pBufferInfo = bufferInfo;
		write.descriptorCount = 1;

		m_Writes.push_back(write);
	}

	// TODO description
	void DescriptorWriter::WriteAs(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* asInfo)
	{
		VL_CORE_ASSERT(m_SetLayout.m_Bindings.count(binding) == 1, "Layout does not contain specified binding : {0}", binding);

		auto& bindingDescription = m_SetLayout.m_Bindings[binding];

		VL_CORE_ASSERT(bindingDescription.descriptorCount == 1, "Writing multiple descriptors is not supported for now. descriptorCount = {0}", bindingDescription.descriptorCount);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.descriptorType;
		write.dstBinding = binding;
		write.descriptorCount = 1;
		write.pNext = asInfo;

		m_Writes.push_back(write);
	}

	/*
	 * @brief Writes image descriptor information to a specified binding in a Vulkan descriptor set.
	 * 
	 * @param binding - The binding index to which the image descriptor information is to be written.
	 * @param imageInfo - Pointer to the VkDescriptorImageInfo structure containing image-specific details.
	 *
	 * @note The 'imageInfo' parameter must remain valid until the DescriptorWriter::Build function is called.
	 */
	void DescriptorWriter::WriteImage(uint32_t binding, VkDescriptorImageInfo* imageInfo)
	{
		VL_CORE_ASSERT(m_SetLayout.m_Bindings.count(binding) == 1, "Layout does not contain specified binding");

		auto& bindingDescription = m_SetLayout.m_Bindings[binding];

		VL_CORE_ASSERT(bindingDescription.descriptorCount == 1, "Binding single descriptor info, but binding expects multiple");

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.descriptorType;
		write.dstBinding = binding;
		write.pImageInfo = imageInfo;
		write.descriptorCount = 1;

		m_Writes.push_back(write);
	}

	/*
	 * @brief Builds a Vulkan descriptor set using the populated descriptor writes(WriteBuffer(), WriteImage()).
	 *
	 * @param set - Reference to the Vulkan descriptor set that will be built (output parameter).
	 *
	 * @return True if the descriptor set is successfully built, false otherwise.
	 */
	bool DescriptorWriter::Build(VkDescriptorSet& set)
	{
		// TODO:
		// Might want to create a "DescriptorPoolManager" class that handles this case, and builds
		// a new pool whenever an old pool fills up.
		VL_CORE_RETURN_ASSERT(m_Pool.AllocateDescriptorSets(m_SetLayout.GetDescriptorSetLayout(), set), 
			true, 
			"Failed to build descriptor. Pool is probably empty."
		);
		Overwrite(set);
		return true;
	}

	/*
	 * @brief Applies accumulated descriptor writes (from WriteImage() and WriteBuffer()) to overwrite a Vulkan descriptor set.
	 *
	 * @param set - Reference to the Vulkan descriptor set to be overwritten.
	 */
	void DescriptorWriter::Overwrite(VkDescriptorSet& set)
	{
		for (auto& write : m_Writes) { write.dstSet = set; }
		vkUpdateDescriptorSets(Device::GetDevice(), (uint32_t)m_Writes.size(), m_Writes.data(), 0, nullptr);
		m_Writes.clear();
	}

}