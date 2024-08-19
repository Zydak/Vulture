// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "pch.h"
#include "DescriptorWriter.h"

namespace Vulture
{

	/**
	 * @brief Initializes the DescriptorWriter with the provided descriptor set layout and descriptor pool.
	 *
	 * @param setLayout - Pointer to the DescriptorSetLayout object representing the descriptor set layout.
	 * @param pool - Pointer to the DescriptorPool object representing the descriptor pool.
	 */
	void DescriptorWriter::Init(DescriptorSetLayout* setLayout, DescriptorPool* pool)
	{
		// Check if the DescriptorWriter has already been initialized.
		if (m_Initialized)
			Destroy(); // If already initialized, destroy the existing state.

		// Store the provided descriptor set layout and descriptor pool.
		m_SetLayout = setLayout;
		m_Pool = pool;

		// Mark the DescriptorWriter as initialized.
		m_Initialized = true;
	}

	/**
	 * @brief Destroys the DescriptorWriter.
	 */
	void DescriptorWriter::Destroy()
	{
		if (!m_Initialized)
			return;

		Reset();
	}

	/**
	 * @brief Constructor for DescriptorWriter.
	 * 
	 * @param setLayout - Pointer to the DescriptorSetLayout object representing the descriptor set layout.
	 * @param pool - Pointer to the DescriptorPool object representing the descriptor pool.
	 */
	DescriptorWriter::DescriptorWriter(DescriptorSetLayout* setLayout, DescriptorPool* pool)
	{
		// Initialize the DescriptorWriter with the provided descriptor set layout and descriptor pool.
		Init(setLayout, pool);
	}

	DescriptorWriter::DescriptorWriter(DescriptorWriter&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_SetLayout		= std::move(other.m_SetLayout);
		m_Pool			= std::move(other.m_Pool);
		m_Writes		= std::move(other.m_Writes);
		m_Initialized	= std::move(other.m_Initialized);

		other.Reset();
	}

	DescriptorWriter& DescriptorWriter::operator=(DescriptorWriter&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_SetLayout = std::move(other.m_SetLayout);
		m_Pool = std::move(other.m_Pool);
		m_Writes = std::move(other.m_Writes);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	/**
	 * @brief Destructor for DescriptorWriter.
	 */
	DescriptorWriter::~DescriptorWriter()
	{
		Destroy();
	}

	/*
	 * @brief Writes buffer descriptor information to a specified binding in a Vulkan descriptor set.
	 *
	 * @param binding - Binding index to which the buffer descriptor information is to be written.
	 * @param bufferInfo - Pointer to the VkDescriptorBufferInfo structure containing buffer-specific details.
	 *
	 * @note The 'bufferInfo' parameter must remain valid until the DescriptorWriter::Build function is called.
	 */
	void DescriptorWriter::WriteBuffer(uint32_t binding, const VkDescriptorBufferInfo* bufferInfo)
	{
		// Check if the DescriptorWriter has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorWriter Not Initialized!");

		// Check if the specified binding exists in the descriptor set layout.
		VL_CORE_ASSERT(m_SetLayout->m_Bindings.size() > binding, "Layout does not contain specified binding: {0}", binding);

		// Retrieve the binding description from the descriptor set layout.
		auto& bindingDescription = m_SetLayout->m_Bindings[binding];

		// Prepare the write descriptor set structure.
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.Type;
		write.dstBinding = binding;
		write.pBufferInfo = bufferInfo;
		write.descriptorCount = bindingDescription.DescriptorsCount;

		// Push the write descriptor set structure to the writes vector.
		m_Writes.push_back(write);
	}

	/**
	 * @brief Writes acceleration structure descriptor information to the DescriptorWriter.
	 *
	 * @param binding - Binding index for which the acceleration structure descriptor information is written.
	 * @param asInfo - Pointer to a VkWriteDescriptorSetAccelerationStructureKHR structure containing acceleration structure descriptor information.
	 *
	 * @note The 'asInfo' parameter must remain valid until the DescriptorWriter::Build function is called.
	 */
	void DescriptorWriter::WriteAs(uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR* asInfo)
	{
		// Check if the DescriptorWriter has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorWriter Not Initialized!");

		// Check if the specified binding exists in the descriptor set layout.
		VL_CORE_ASSERT(m_SetLayout->m_Bindings.size() > binding, "Layout does not contain specified binding: {0}", binding);

		// Retrieve the binding description from the descriptor set layout.
		auto& bindingDescription = m_SetLayout->m_Bindings[binding];

		// Prepare the write descriptor set structure.
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.Type;
		write.dstBinding = binding;
		write.descriptorCount = bindingDescription.DescriptorsCount;
		write.pNext = asInfo;

		// Push the write descriptor set structure to the writes vector.
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
	void DescriptorWriter::WriteImage(uint32_t binding, const VkDescriptorImageInfo* imageInfo)
	{
		// Check if the DescriptorWriter has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorWriter Not Initialized!");

		// Check if the specified binding exists in the descriptor set layout.
		VL_CORE_ASSERT(m_SetLayout->m_Bindings.size() > binding, "Layout does not contain specified binding: {0}", binding);

		// Retrieve the binding description from the descriptor set layout.
		auto& bindingDescription = m_SetLayout->m_Bindings[binding];

		// Prepare the write descriptor set structure.
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.Type;
		write.dstBinding = binding;
		write.pImageInfo = imageInfo;
		write.descriptorCount = bindingDescription.DescriptorsCount;

		// Push the write descriptor set structure to the writes vector.
		m_Writes.push_back(write);
	}

	/*
	 * @brief Builds a Vulkan descriptor set using the populated descriptor writes (WriteBuffer(), WriteImage(), etc.).
	 *
	 * @param set - Pointer to the Vulkan descriptor set that will be built.
	 * @param allocateNewSet - Flag that dictates whether to allocate new set, set to false if set is already allocated.
	 *
	 * @return True if the descriptor set is successfully built, false otherwise.
	 */
	bool DescriptorWriter::Build(VkDescriptorSet* set, bool allocateNewSet)
	{
		// Check if the DescriptorWriter has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorWriter Not Initialized!");

		if (allocateNewSet)
		{
			// Attempt to allocate a descriptor set from the descriptor pool using the descriptor set layout.
			VL_CORE_RETURN_ASSERT(m_Pool->AllocateDescriptorSets(m_SetLayout->GetDescriptorSetLayoutHandle(), set),
				true,
				"Failed to build descriptor. Pool is probably empty."
			);
		}

		// Overwrite the allocated descriptor set with the accumulated write operations.
		Overwrite(set);

		// Return true indicating successful descriptor set allocation and write operations.
		return true;
	}

	/*
	 * @brief Applies accumulated descriptor writes (from WriteImage() and WriteBuffer()) to overwrite a Vulkan descriptor set.
	 *
	 * @param set - Reference to the Vulkan descriptor set to be overwritten.
	 */
	void DescriptorWriter::Overwrite(VkDescriptorSet* set)
	{
		// Check if the DescriptorWriter has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorWriter Not Initialized!");

		// Assign the provided descriptor set to each write operation in the writes vector.
		for (auto& write : m_Writes)
		{
			write.dstSet = *set;
		}

		// Update the descriptor sets with the accumulated write operations.
		vkUpdateDescriptorSets(Device::GetDevice(), static_cast<uint32_t>(m_Writes.size()), m_Writes.data(), 0, nullptr);

		// Clear the writes vector.
		m_Writes.clear();
	}

	void DescriptorWriter::Reset()
	{
		m_SetLayout = nullptr;
		m_Pool = nullptr;
		m_Writes.clear();
	}

}