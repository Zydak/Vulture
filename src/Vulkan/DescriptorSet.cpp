#include "pch.h"
#include "Utility/Utility.h"

#include "DescriptorSet.h"

namespace Vulture
{
	/**
	 * @brief Initializes the descriptor set with the provided pool and bindings.
	 * 
	 * @param pool - Pointer to the descriptor pool to allocate the descriptor set from.
	 * @param bindings - Descriptor bindings specifying the layout of the descriptor set.
	 */
	void DescriptorSet::Init(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings)
	{
		// Check if the descriptor set has already been initialized.
		if (m_Initialized)
			Destroy();

		// Set the descriptor pool.
		m_Pool = pool;

		// Initialize the descriptor set layout with the provided bindings.
		m_DescriptorSetLayout.Init(bindings);

		m_Initialized = true;
	}

	/**
	 * @brief Destroys the descriptor set.
	 */
	void DescriptorSet::Destroy()
	{
		// Destroy the descriptor set layout.
		m_DescriptorSetLayout.Destroy();

		// Clear the bindings write information.
		m_BindingsWriteInfo.clear();

		// Reset the descriptor pool pointer.
		m_Pool = nullptr;

		// Clear the buffer bindings.
		m_Buffers.clear();

		// Mark the descriptor set as uninitialized.
		m_Initialized = false;
	}

	/**
	 * @brief Constructor for the DescriptorSet class.
	 *
	 * @param pool - Pointer to the descriptor pool to allocate the descriptor set from.
	 * @param bindings - Descriptor bindings specifying the layout of the descriptor set.
	 */
	DescriptorSet::DescriptorSet(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings)
	{
		// Initialize the descriptor set with the provided pool and bindings.
		Init(pool, bindings);
	}

	/**
	 * @brief Destructor for the DescriptorSet class.
	 */
	DescriptorSet::~DescriptorSet()
	{
		// If the descriptor set is initialized, destroy it.
		if (m_Initialized)
			Destroy();
	}

	/**
	 * @brief Adds an image sampler to the descriptor set binding.
	 * 
	 * @param binding - Binding number to which the image sampler will be added.
	 * @param sampler - VkSampler handle for the image sampler.
	 * @param imageView - VkImageView handle for the image view.
	 * @param imageLayout - Layout of the image in the descriptor.
	 */
	void DescriptorSet::AddImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
	{
		// Check if the DescriptorSet has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Check if the binding number is valid.
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		// Check if the binding type in the descriptor set layout matches VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER or VK_DESCRIPTOR_TYPE_STORAGE_IMAGE.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
			"Wrong Binding Type! Type inside layout is: {}", m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type
		);

		// Check if the number of descriptors exceeds the specified count for the binding in the descriptor set layout.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount > m_BindingsWriteInfo[binding].m_ImageInfo.size(),
			"Too many descriptors! Descriptor Count specified in layout: {0}",
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount
		);

		// Create a descriptor image info structure.
		VkDescriptorImageInfo imageDescriptor{};
		imageDescriptor.sampler = sampler;
		imageDescriptor.imageView = imageView;
		imageDescriptor.imageLayout = imageLayout;

		// Add the image sampler to the bindings write information for the specified binding.
		m_BindingsWriteInfo[binding].m_ImageInfo.push_back(imageDescriptor);

		// Set the binding type to image.
		m_BindingsWriteInfo[binding].m_Type = BindingType::Image;
	}

	/**
	 * @brief Adds an acceleration structure to the specified descriptor set binding.
	 *
	 * @param binding - Binding number to which the acceleration structure will be added.
	 * @param asInfo - VkWriteDescriptorSetAccelerationStructureKHR structure containing acceleration structure info.
	 */
	void DescriptorSet::AddAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR asInfo)
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Check if the binding number is valid.
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		// Check if the binding type in the descriptor set layout matches VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
			"Wrong Binding Type! Type inside layout is: {}", m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type
		);

		// Check if the number of descriptors exceeds the specified count for the binding in the descriptor set layout.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount > m_BindingsWriteInfo[binding].m_AccelInfo.size(),
			"Too many descriptors! Descriptor Count specified in layout: {0}",
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount
		);

		// Add the acceleration structure to the bindings write information for the specified binding.
		m_BindingsWriteInfo[binding].m_AccelInfo.push_back(asInfo);

		// Set the binding type to acceleration structure.
		m_BindingsWriteInfo[binding].m_Type = BindingType::AS;
	}

	/*
	 * @brief Adds a uniform buffer to the descriptor set.
	 *
	 * @param binding - The binding point for the uniform buffer.
	 * @param bufferSize - The size of the uniform buffer.
	 * @param stage - The shader stage where the uniform buffer will be used.
	 * @param deviceLocal - Flag indicating whether the buffer should be device-local or host-visible.
	 */
	void DescriptorSet::AddUniformBuffer(uint32_t binding, VkDeviceSize bufferSize, bool deviceLocal)
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Check if the binding number is valid.
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		// Check if the binding type in the descriptor set layout matches VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			"Wrong Binding Type! Type inside layout is: {}", m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type
		);

		// Check if the number of descriptors exceeds the specified count for the binding in the descriptor set layout.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount > m_BindingsWriteInfo[binding].m_BufferInfo.size(),
			"Too many descriptors! Descriptor Count specified in layout: {0}",
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].DescriptorsCount
		);

		// Determine buffer usage flags based on deviceLocal flag.
		VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (deviceLocal) 
		{
			bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}

		// Create a new uniform buffer and initialize it.
		m_Buffers[binding].push_back(Vulture::Buffer());
		Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = bufferSize;
		info.UsageFlags = bufferUsageFlags;
		info.MemoryPropertyFlags = deviceLocal ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		m_Buffers[binding][m_Buffers[binding].size() - 1].Init(info);
		m_Buffers[binding][m_Buffers[binding].size() - 1].Map();

		// Add the buffer descriptor info to the bindings write information for the specified binding.
		m_BindingsWriteInfo[binding].m_BufferInfo.push_back(m_Buffers[binding][m_Buffers[binding].size() - 1].DescriptorInfo());

		// Set the binding type to buffer.
		m_BindingsWriteInfo[binding].m_Type = BindingType::Buffer;
	}

	/**
	 * @brief Adds a storage buffer to the descriptor set binding.
	 *
	 * @param binding - Binding number to which the storage buffer will be added.
	 * @param bufferSize - Size of the storage buffer.
	 * @param resizable - Flag indicating whether the buffer should be resizable.
	 * @param deviceLocal - Flag indicating whether the buffer should be device-local or host-visible.
	 */
	void DescriptorSet::AddStorageBuffer(uint32_t binding, VkDeviceSize bufferSize, bool resizable, bool deviceLocal)
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Check if the binding number is valid.
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		// Check if the binding type in the descriptor set layout matches VK_DESCRIPTOR_TYPE_STORAGE_BUFFER.
		VL_CORE_ASSERT(
			m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			"Wrong Binding Type! Type inside layout is: {}", m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type
		);

		// Determine buffer usage flags based on resizable and deviceLocal flags.
		VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (resizable)
		{
			bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
		if (deviceLocal && !resizable) 
		{
			bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}

		// Initialize the buffer.
		Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = bufferSize;
		info.UsageFlags = bufferUsageFlags;
		info.MemoryPropertyFlags = deviceLocal ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		
		m_Buffers[binding].push_back(Vulture::Buffer());
		m_Buffers[binding][m_Buffers[binding].size() - 1].Init(info);

		// Map the buffer if it's not device-local.
		if (!deviceLocal) 
		{
			m_Buffers[binding][m_Buffers[binding].size() - 1].Map();
		}

		// Add the buffer descriptor info to the bindings write information for the specified binding.
		m_BindingsWriteInfo[binding].m_BufferInfo.push_back(m_Buffers[binding][m_Buffers[binding].size() - 1].DescriptorInfo());

		// Set the binding type to buffer.
		m_BindingsWriteInfo[binding].m_Type = BindingType::Buffer;
	}

	/**
	 * @brief Builds the descriptor set.
	 */
	void DescriptorSet::Build()
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Create a descriptor writer using the descriptor set layout and the descriptor pool.
		DescriptorWriter writer(&m_DescriptorSetLayout, m_Pool);

		// Iterate over each binding and write data to the descriptor set.
		for (auto& binding : m_BindingsWriteInfo)
		{
			// Determine the type of binding and call the corresponding write function.
			switch (binding.second.m_Type)
			{
				case BindingType::Image:
				{
					writer.WriteImage(binding.first, binding.second.m_ImageInfo.data());
				}
				break;

				case BindingType::Buffer:
				{
					writer.WriteBuffer(binding.first, binding.second.m_BufferInfo.data());
				}
				break;

				case BindingType::AS:
				{
					writer.WriteAs(binding.first, binding.second.m_AccelInfo.data());
				}
				break;

				default:
					// This case should not happen. If it does, it indicates an unknown binding type.
					VL_CORE_ASSERT(false, "Unknown binding type: {0}", binding.second.m_Type);
					break;
			}
		}

		// Build the descriptor set using the descriptor writer and store the handle.
		writer.Build(&m_DescriptorSetHandle);
	}

	/**
	 * @brief Resize the storage buffer associated with a binding in the descriptor set.
	 *
	 * @param binding - Binding index inside descriptor set.
	 * @param newSize - New size of the storage buffer.
	 * @param commandBuffer (Optional) - Vulkan cmd buffer that copy command will be recorded onto.
	 * @param buffer (Optional) - If there are multiple buffers in single binding, index of the buffer within the binding must be specified.
	 * 
	 * @note All data within the buffer is lost after resize
	 */
	void DescriptorSet::Resize(uint32_t binding, VkDeviceSize newSize, VkCommandBuffer commandBuffer, uint32_t buffer)
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Check if the specified binding exists.
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		// Check if the buffer is resizable and the binding is not an image or uniform buffer.
		VL_CORE_ASSERT(m_Buffers[binding][buffer].GetUsageFlags() & VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "Resizable flag has to be set!");
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, "Only storage buffers can be resized");
		
		// Create a copy of the old buffer.
		Vulture::Buffer::CreateInfo oldInfo = m_Buffers[binding][buffer].GetCreateInfo();
		
		// Initialize a new buffer with the new size and properties.
		Vulture::Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = newSize;
		info.UsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		if (oldInfo.MemoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			info.MemoryPropertyFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		else
			info.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
		m_Buffers[binding][buffer].Init(info);
		if ((oldInfo.MemoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0)
			m_Buffers[binding][buffer].Map();

		m_BindingsWriteInfo[binding].m_BufferInfo[buffer] = m_Buffers[binding][buffer].DescriptorInfo();

		// Create a descriptor writer and update the descriptor set.
		DescriptorWriter writer(&m_DescriptorSetLayout, m_Pool);
		writer.WriteBuffer(binding, m_BindingsWriteInfo[binding].m_BufferInfo.data());
		writer.Build(&m_DescriptorSetHandle, false);
	}

	/*
	 * @brief Updates an image sampler on corresponding binding in the descriptor set with the specified image view, sampler, 
	 * and image layout. It utilizes a descriptor writer to perform the update and then overwrites 
	 * the descriptor set to reflect the changes.
	 *
	 * @param binding - Binding index of the image sampler to be updated.
	 * @param imageView - Vulkan image view to be associated with the image sampler.
	 * @param sampler - Vulkan sampler to be associated with the image sampler.
	 * @param layout - Vulkan image layout to be associated with the image sampler.
	 */
	void DescriptorSet::UpdateImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout layout)
	{
		// Check if the descriptor set has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		// Create a descriptor writer.
		DescriptorWriter writer(&m_DescriptorSetLayout, m_Pool);

		// Create a descriptor image info structure.
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = layout;
		imageInfo.imageView = imageView;
		imageInfo.sampler = sampler;

		// Write the image descriptor to the specified binding.
		writer.WriteImage(binding, &imageInfo);

		// Update the descriptor set with the new descriptor.
		writer.Overwrite(&m_DescriptorSetHandle);
	}

	/*
	 * @brief Binds the descriptor set to a Vulkan command buffer.
	 *
	 * @param set - Index of the descriptor set to bind to.
	 * @param layout - Vulkan pipeline layout to use for the binding.
	 * @param bindPoint - Vulkan pipeline bind point (e.g., VK_PIPELINE_BIND_POINT_GRAPHICS or VK_PIPELINE_BIND_POINT_COMPUTE).
	 * @param cmdBuffer - Vulkan command buffer to which the descriptor set is bound.
	 */
	void DescriptorSet::Bind(const uint32_t& set, const VkPipelineLayout& layout, const VkPipelineBindPoint& bindPoint, const VkCommandBuffer& cmdBuffer) const
	{
		// Check if the DescriptorSet has been initialized.
		VL_CORE_ASSERT(m_Initialized, "DescriptorSet Not Initialized!");

		vkCmdBindDescriptorSets(
			cmdBuffer,
			bindPoint,
			layout,
			set,
			1,
			&m_DescriptorSetHandle,
			0,
			nullptr
		);
	}
}