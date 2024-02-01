#include "pch.h"
#include "Utility/Utility.h"

#include "Uniform.h"

namespace Vulture
{

	Uniform::Uniform(DescriptorPool& pool)
		: m_Pool(pool)
	{}

	Uniform::~Uniform()
	{

	}

	// TODO multiple descriptors in single uniform, especially for layouts alone
	/**
	 * @brief Adds a descriptor for an image sampler to the uniform.
	 *
	 * @param binding - The binding point for the image sampler.
	 * @param sampler - The Vulkan sampler to be used.
	 * @param imageView - The Vulkan image view to be used.
	 * @param imageLayout - The layout of the image.
	 * @param stage - The shader stage where the sampler will be used.
	 * @param count - number of descriptors in binding.
	 * @param type - The descriptor type for the binding.
	 */
	void Uniform::AddImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout, VkShaderStageFlagBits stage, int count, VkDescriptorType Type)
	{
		//VL_CORE_ASSERT(m_Bindings.count(binding) == 0, "Binding already in use!");
		if (m_Bindings.count(binding) != 0) // Binding Found, add to existing binding
		{
			VkDescriptorImageInfo imageDescriptor{};
			imageDescriptor.sampler = sampler;
			imageDescriptor.imageView = imageView;
			imageDescriptor.imageLayout = imageLayout;

			m_Bindings[binding].ImageInfo.push_back(imageDescriptor);

			return;
		}
		else // Binding not found, create new binding
		{
			m_SetBuilder.AddBinding(binding, Type, stage, count);

			VkDescriptorImageInfo imageDescriptor{};
			imageDescriptor.sampler = sampler;
			imageDescriptor.imageView = imageView;
			imageDescriptor.imageLayout = imageLayout;

			Binding bin;
			bin.ImageInfo.push_back(imageDescriptor);
			bin.Type = BindingType::Image;
			m_Bindings[binding] = bin;

			return;
		}
	}

	// TODO description
	void Uniform::AddAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR asInfo)
	{
		VL_CORE_ASSERT(m_Bindings.count(binding) == 0, "Binding already in use!");
		m_SetBuilder.AddBinding(binding, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

		Binding bin;
		bin.AccelInfo = asInfo;
		bin.Type = BindingType::AccelerationStructure;
		m_Bindings[binding] = bin;
	}

	/*
	 * @brief Adds a descriptor for a uniform buffer to the uniform.
	 *
	 * @param binding - The binding point for the uniform buffer.
	 * @param bufferSize - The size of the uniform buffer.
	 * @param stage - The shader stage where the uniform buffer will be used.
	 */
	void Uniform::AddUniformBuffer(uint32_t binding, uint32_t bufferSize, VkShaderStageFlags stage)
	{
		VL_CORE_ASSERT(m_Bindings.count(binding) == 0, "Binding already in use!");
		VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		m_Buffers[binding] = Buffer();
		Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = bufferSize;
		info.UsageFlags = bufferUsageFlags;
		info.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // TODO ability to create device local buffers?
		m_Buffers[binding].Init(info);
		m_Buffers[binding].Map();
		m_SetBuilder.AddBinding(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);

		Binding bin;
		bin.BufferInfo = m_Buffers[binding].DescriptorInfo();
		bin.Type = BindingType::Buffer;
		m_Bindings[binding] = bin;
	}

	/*
	 * @brief Adds a descriptor for a storage buffer to the uniform.
	 *
	 * @param binding - The binding point for the storage buffer.
	 * @param bufferSize - The size of the storage buffer.
	 * @param stage - The shader stage where the storage buffer will be used.
	 * @param resizable - A flag indicating whether the storage buffer is resizable.
	 */
	void Uniform::AddStorageBuffer(uint32_t binding, uint32_t bufferSize, VkShaderStageFlagBits stage, bool resizable)
	{
		VL_CORE_ASSERT(m_Bindings.count(binding) == 0, "Binding already in use!");
		VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (resizable) { bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT; }
		m_Buffers[binding] = Buffer();
		Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = bufferSize;
		info.UsageFlags = bufferUsageFlags;
		info.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // TODO ability to create device local buffers?
		m_Buffers[binding].Init(info);
		m_Buffers[binding].Map();
		m_SetBuilder.AddBinding(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);

		Binding bin;
		bin.BufferInfo = m_Buffers[binding].DescriptorInfo();
		bin.Type = BindingType::Buffer;
		m_Bindings[binding] = bin;
	}

	/**
	 * @brief Builds the uniform by creating a descriptor set layout and writing descriptor bindings based on 
	 * the specified bindings. It iterates over the bindings, and for each binding, it uses a descriptor writer to write the 
	 * corresponding descriptor information to the descriptor set.
	 */
	void Uniform::Build()
	{
		m_DescriptorSetLayout = std::make_shared<DescriptorSetLayout>(m_SetBuilder);

		DescriptorWriter writer(*m_DescriptorSetLayout, m_Pool);
		for (int i = 0; i < (int)m_Bindings.size(); i++)
		{
			switch (m_DescriptorSetLayout->GetDescriptorSetLayoutBindings()[i].descriptorType)
			{

			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
				writer.WriteImage(i, m_Bindings[i].ImageInfo.data());
			} break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
				writer.WriteImage(i, m_Bindings[i].ImageInfo.data());
			} break;

			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
				writer.WriteBuffer(i, &m_Bindings[i].BufferInfo);
			} break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
				writer.WriteBuffer(i, &m_Bindings[i].BufferInfo);
			} break;

			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
				writer.WriteAs(i, &m_Bindings[i].AccelInfo);
			} break;

			default: VL_CORE_ASSERT(false, "Binding Type not supported!"); break;
			}
		}

		writer.Build(m_DescriptorSet);
	}

	/**
	 * @brief Resizes a buffer on corresponding binding in the uniform. It checks the validity of the specified binding and performs 
	 * the necessary operations to resize the buffer.
	 *
	 * @param binding - The binding index of the buffer to be resized.
	 * @param newSize - The new size of the buffer.
	 * @param queue - The Vulkan queue to execute commands on.
	 * @param pool - The Vulkan command pool to allocate command buffers from.
	 */
	void Uniform::Resize(uint32_t binding, uint32_t newSize, VkQueue queue, VkCommandPool pool)
	{
		VL_CORE_ASSERT(binding < m_DescriptorSetLayout->GetDescriptorSetLayoutBindings().size(), "there isn't such binding! binding: {0}", binding);
		VL_CORE_ASSERT(m_Buffers[binding].GetUsageFlags() & VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "Resizable flag has to be set!");
		VL_CORE_ASSERT(m_DescriptorSetLayout->GetDescriptorSetLayoutBindings()[binding].descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, "Can't resize an image!");
		VL_CORE_ASSERT(m_DescriptorSetLayout->GetDescriptorSetLayoutBindings()[binding].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "Binding has to be storage buffer!");

		Buffer* oldBuffer = &m_Buffers[binding];
		m_Buffers[binding] = Buffer();
		Buffer::CreateInfo info{};
		info.InstanceCount = 1;
		info.InstanceSize = newSize;
		info.UsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		info.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT; // TODO ability to create device local buffers?
		m_Buffers[binding].Init(info);
		m_Buffers[binding].Map();
		m_Bindings[binding].BufferInfo = m_Buffers[binding].DescriptorInfo();

		// TODO: check whether we actually need to write to all bindings at resize
		DescriptorWriter writer(*m_DescriptorSetLayout, m_Pool);
		for (int i = 0; i < (int)m_DescriptorSetLayout->GetDescriptorSetLayoutBindings().size(); i++) {

			switch (m_DescriptorSetLayout->GetDescriptorSetLayoutBindings()[i].descriptorType) {
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
				writer.WriteImage(i, m_Bindings[i].ImageInfo.data());
			} break;

			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
				writer.WriteBuffer(i, &m_Bindings[i].BufferInfo);
			} break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
				writer.WriteBuffer(i, &m_Bindings[i].BufferInfo);
			} break;

			case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
				writer.WriteAs(i, &m_Bindings[i].AccelInfo);
			} break;

			default: VL_CORE_ASSERT(false, "Binding Type not supported!"); break;
			}
		}

		writer.Build(m_DescriptorSet);
		Buffer::CopyBuffer(oldBuffer->GetBuffer(), m_Buffers[binding].GetBuffer(), oldBuffer->GetBufferSize(), queue, pool);
	}

	/*
	 * @brief Updates an image sampler on corresponding binding in the uniform with the specified image view, sampler, 
	 * and image layout. It utilizes a descriptor writer to perform the update and then overwrites 
	 * the descriptor set to reflect the changes.
	 *
	 * @param binding - The binding index of the image sampler to be updated.
	 * @param imageView - The Vulkan image view to be associated with the image sampler.
	 * @param sampler - The Vulkan sampler to be associated with the image sampler.
	 * @param layout - The Vulkan image layout to be associated with the image sampler.
	 */
	void Uniform::UpdateImageSampler(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout)
	{
		DescriptorWriter writer(*m_DescriptorSetLayout, m_Pool);
		VkDescriptorImageInfo ImageInfo;
		ImageInfo.imageLayout = layout;
		ImageInfo.imageView = imageView;
		ImageInfo.sampler = sampler;
		writer.WriteImage(binding, &ImageInfo);
		writer.Overwrite(m_DescriptorSet);
	}

	/*
	 * @brief Binds the uniform descriptor set to a Vulkan command buffer.
	 *
	 * @param set - The index of the descriptor set to bind to.
	 * @param layout - The Vulkan pipeline layout to use for the binding.
	 * @param bindPoint - The Vulkan pipeline bind point (e.g., VK_PIPELINE_BIND_POINT_GRAPHICS or VK_PIPELINE_BIND_POINT_COMPUTE).
	 * @param cmdBuffer - The Vulkan command buffer to which the descriptor set is bound.
	 */
	void Uniform::Bind(const uint32_t& set, const VkPipelineLayout& layout, const VkPipelineBindPoint& bindPoint, const VkCommandBuffer& cmdBuffer)
	{
		vkCmdBindDescriptorSets(
			cmdBuffer,
			bindPoint,
			layout,
			set,
			1,
			&m_DescriptorSet,
			0,
			nullptr
		);
	}
}