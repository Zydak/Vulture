#include "pch.h"
#include "Utility/Utility.h"

#include "DescriptorSet.h"

namespace Vulture
{

	void DescriptorSet::Init(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings)
	{
		m_Pool = pool;

		m_DescriptorSetLayout.Init(bindings);
	}

	void DescriptorSet::Destroy()
	{
		m_Initialized = false;
	}

	DescriptorSet::DescriptorSet(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings)
	{
		Init(pool, bindings);
	}

	DescriptorSet::~DescriptorSet()
	{
		if (m_Initialized)
			Destroy();
	}

	/**
	 * @brief Adds a descriptor for an image sampler to the descriptor set.
	 *
	 * @param binding - The binding point for the image sampler.
	 * @param sampler - The Vulkan sampler to be used.
	 * @param imageView - The Vulkan image view to be used.
	 * @param imageLayout - The layout of the image.
	 * @param stage - The shader stage where the sampler will be used.
	 * @param count - number of descriptors in binding.
	 * @param type - The descriptor type for the binding.
	 */
	void DescriptorSet::AddImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
	{
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		VkDescriptorImageInfo imageDescriptor{};
		imageDescriptor.sampler = sampler;
		imageDescriptor.imageView = imageView;
		imageDescriptor.imageLayout = imageLayout;

		m_BindingsWriteInfo[binding].m_ImageInfo.push_back(imageDescriptor);
		m_BindingsWriteInfo[binding].m_Type = BindingType::Image;
	}

	// TODO description
	void DescriptorSet::AddAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR asInfo)
	{
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		m_BindingsWriteInfo[binding].m_AccelInfo.push_back(asInfo);
		m_BindingsWriteInfo[binding].m_Type = BindingType::AS;
	}

	/*
	 * @brief Adds a uniform buffer to the descriptor set.
	 *
	 * @param binding - The binding point for the uniform buffer.
	 * @param bufferSize - The size of the uniform buffer.
	 * @param stage - The shader stage where the uniform buffer will be used.
	 */
	void DescriptorSet::AddUniformBuffer(uint32_t binding, uint32_t bufferSize)
	{
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		m_Buffers[binding].push_back(Vulture::Buffer());
		Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = bufferSize;
		info.UsageFlags = bufferUsageFlags;
		info.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // TODO ability to create device local buffers?
		m_Buffers[binding][m_Buffers[binding].size()-1].Init(info);
		m_Buffers[binding][m_Buffers[binding].size()-1].Map();

		m_BindingsWriteInfo[binding].m_BufferInfo.push_back(m_Buffers[binding][m_Buffers[binding].size()-1].DescriptorInfo());
		m_BindingsWriteInfo[binding].m_Type = BindingType::Buffer;
	}

	/*
	 * @brief Adds a descriptor for a storage buffer to the descriptor set.
	 *
	 * @param binding - The binding point for the storage buffer.
	 * @param bufferSize - The size of the storage buffer.
	 * @param stage - The shader stage where the storage buffer will be used.
	 * @param resizable - A flag indicating whether the storage buffer is resizable.
	 */
	void DescriptorSet::AddStorageBuffer(uint32_t binding, uint32_t bufferSize, bool resizable)
	{
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");

		VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (resizable) { bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT; }
		m_Buffers[binding].push_back(Vulture::Buffer());
		Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = bufferSize;
		info.UsageFlags = bufferUsageFlags;
		info.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // TODO ability to create device local buffers?
		m_Buffers[binding][m_Buffers[binding].size() - 1].Init(info);
		m_Buffers[binding][m_Buffers[binding].size() - 1].Map();

		m_BindingsWriteInfo[binding].m_BufferInfo.push_back(m_Buffers[binding][m_Buffers[binding].size() - 1].DescriptorInfo());
		m_BindingsWriteInfo[binding].m_Type = BindingType::Buffer;
	}

	/**
	 * @brief Builds the descriptor set by creating a descriptor set layout and writing descriptor bindings based on 
	 * the specified bindings. It iterates over the bindings, and for each binding, it uses a descriptor writer to write the 
	 * corresponding descriptor information to the descriptor set.
	 */
	void DescriptorSet::Build()
	{
		DescriptorWriter writer(&m_DescriptorSetLayout, m_Pool);
		for (int i = 0; i < (int)m_BindingsWriteInfo.size(); i++)
		{
			switch (m_BindingsWriteInfo[i].m_Type)
			{
			case BindingType::Image: 
			{
				writer.WriteImage(i, m_BindingsWriteInfo[i].m_ImageInfo.data());
			} break;

			case BindingType::Buffer: 
			{
				writer.WriteBuffer(i, m_BindingsWriteInfo[i].m_BufferInfo.data());
			} break;

			case BindingType::AS:
			{
				writer.WriteAs(i, m_BindingsWriteInfo[i].m_AccelInfo.data());
			} break;

			default: VL_CORE_ASSERT(false, "Unknown binding type: {0}", m_BindingsWriteInfo[i].m_Type); break;
			}
		}

		writer.Build(m_DescriptorSetHandle);
	}

	/**
	 * @brief Resizes a buffer on corresponding binding in the descriptor set. It checks the validity of the specified binding and performs 
	 * the necessary operations to resize the buffer.
	 *
	 * @param binding - The binding index of the buffer to be resized.
	 * @param newSize - The new size of the buffer.
	 * @param queue - The Vulkan queue to execute commands on.
	 * @param pool - The Vulkan command pool to allocate command buffers from.
	 */
	void DescriptorSet::Resize(uint32_t binding, uint32_t newSize, VkQueue queue, VkCommandPool pool, uint32_t buffer)
	{
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings().size() >= binding, "There is no such binding: {0}");
		VL_CORE_ASSERT(m_Buffers[binding][buffer].GetUsageFlags() & VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "Resizable flag has to be set!");
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, "Can't resize an image!");
		VL_CORE_ASSERT(m_DescriptorSetLayout.GetDescriptorSetLayoutBindings()[binding].Type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "Binding has to be storage buffer!");

		Vulture::Buffer oldBuffer = m_Buffers[binding][buffer];
		Vulture::Buffer::CopyBuffer(m_Buffers[binding][buffer].GetBuffer(), oldBuffer.GetBuffer(), oldBuffer.GetBufferSize(), Device::GetGraphicsQueue(), 0, Device::GetGraphicsCommandPool());
		m_Buffers[binding][buffer] = Vulture::Buffer();
		Vulture::Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = newSize;
		info.UsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		info.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // TODO ability to create device local buffers?
		m_Buffers[binding][buffer].Init(info);
		m_Buffers[binding][buffer].Map();
		m_BindingsWriteInfo[binding].m_BufferInfo[buffer] = m_Buffers[binding][buffer].DescriptorInfo();

		DescriptorWriter writer(&m_DescriptorSetLayout, m_Pool);
		writer.WriteBuffer(binding, m_BindingsWriteInfo[binding].m_BufferInfo.data());
		
		writer.Build(m_DescriptorSetHandle);
		Buffer::CopyBuffer(oldBuffer.GetBuffer(), m_Buffers[binding][buffer].GetBuffer(), oldBuffer.GetBufferSize(), queue, 0, pool);
	}

	/*
	 * @brief Updates an image sampler on corresponding binding in the descriptor set with the specified image view, sampler, 
	 * and image layout. It utilizes a descriptor writer to perform the update and then overwrites 
	 * the descriptor set to reflect the changes.
	 *
	 * @param binding - The binding index of the image sampler to be updated.
	 * @param imageView - The Vulkan image view to be associated with the image sampler.
	 * @param sampler - The Vulkan sampler to be associated with the image sampler.
	 * @param layout - The Vulkan image layout to be associated with the image sampler.
	 */
	void DescriptorSet::UpdateImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout layout)
	{
		DescriptorWriter writer(&m_DescriptorSetLayout, m_Pool);
		VkDescriptorImageInfo ImageInfo;
		ImageInfo.imageLayout = layout;
		ImageInfo.imageView = imageView;
		ImageInfo.sampler = sampler;
		writer.WriteImage(binding, &ImageInfo);
		writer.Overwrite(m_DescriptorSetHandle);
	}

	/*
	 * @brief Binds the descriptor set to a Vulkan command buffer.
	 *
	 * @param set - The index of the descriptor set to bind to.
	 * @param layout - The Vulkan pipeline layout to use for the binding.
	 * @param bindPoint - The Vulkan pipeline bind point (e.g., VK_PIPELINE_BIND_POINT_GRAPHICS or VK_PIPELINE_BIND_POINT_COMPUTE).
	 * @param cmdBuffer - The Vulkan command buffer to which the descriptor set is bound.
	 */
	void DescriptorSet::Bind(const uint32_t& set, const VkPipelineLayout& layout, const VkPipelineBindPoint& bindPoint, const VkCommandBuffer& cmdBuffer)
	{
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