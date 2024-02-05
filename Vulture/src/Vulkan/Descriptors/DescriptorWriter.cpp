#include "pch.h"
#include "DescriptorWriter.h"

namespace Vulture
{

	void DescriptorWriter::Init(DescriptorSetLayout* setLayout, DescriptorPool* pool)
	{
		m_SetLayout = setLayout;
		m_Pool = pool;
	}

	DescriptorWriter::DescriptorWriter(DescriptorSetLayout* setLayout, DescriptorPool* pool)
	{
		Init(setLayout, pool);
	}

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
		VL_CORE_ASSERT(m_SetLayout->m_Bindings.size() >= binding, "Layout does not contain specified binding : {0}", binding);

		auto& bindingDescription = m_SetLayout->m_Bindings[binding];

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.Type;
		write.dstBinding = binding;
		write.pBufferInfo = bufferInfo;
		write.descriptorCount = bindingDescription.DescriptorsCount;

		m_Writes.push_back(write);
	}

	// TODO description
	void DescriptorWriter::WriteAs(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* asInfo)
	{
		VL_CORE_ASSERT(m_SetLayout->m_Bindings.size() >= binding, "Layout does not contain specified binding : {0}", binding);

		auto& bindingDescription = m_SetLayout->m_Bindings[binding];

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.Type;
		write.dstBinding = binding;
		write.descriptorCount = bindingDescription.DescriptorsCount;
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
		VL_CORE_ASSERT(m_SetLayout->m_Bindings.size() >= binding, "Layout does not contain specified binding : {0}", binding);

		auto& bindingDescription = m_SetLayout->m_Bindings[binding];

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.Type;
		write.dstBinding = binding;
		write.pImageInfo = imageInfo;
		write.descriptorCount = bindingDescription.DescriptorsCount;

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
		// build a new pool whenever an old pool fills up?
		VL_CORE_RETURN_ASSERT(m_Pool->AllocateDescriptorSets(m_SetLayout->GetDescriptorSetLayout(), set),
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