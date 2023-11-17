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

	void Uniform::AddImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout, VkShaderStageFlagBits stage, VkDescriptorType Type)
	{
		VL_CORE_ASSERT(m_Bindings.count(binding) == 0, "Binding already in use!");
		VkDescriptorImageInfo imageDescriptor{};
		imageDescriptor.sampler = sampler;
		imageDescriptor.imageView = imageView;
		imageDescriptor.imageLayout = imageLayout;
		m_SetBuilder.AddBinding(binding, Type, stage);

		Binding bin;
		bin.ImageInfo = imageDescriptor;
		bin.Type = BindingType::Image;
		m_Bindings[binding] = bin;
	}

	void Uniform::AddUniformBuffer(uint32_t binding, uint32_t bufferSize, VkShaderStageFlagBits stage)
	{
		VL_CORE_ASSERT(m_Bindings.count(binding) == 0, "Binding already in use!");
		VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		m_Buffers[binding] = std::make_shared<Buffer>(bufferSize, 1, bufferUsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		m_Buffers[binding]->Map();
		m_SetBuilder.AddBinding(binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage);

		Binding bin;
		bin.BufferInfo = m_Buffers[binding]->DescriptorInfo();
		bin.Type = BindingType::Buffer;
		m_Bindings[binding] = bin;
	}

	void Uniform::AddStorageBuffer(uint32_t binding, uint32_t bufferSize, VkShaderStageFlagBits stage, bool resizable)
	{
		VL_CORE_ASSERT(m_Bindings.count(binding) == 0, "Binding already in use!");
		VkBufferUsageFlags bufferUsageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (resizable) { bufferUsageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT; }
		m_Buffers[binding] = (std::make_shared<Buffer>(bufferSize, 1, bufferUsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
		m_Buffers[binding]->Map();
		m_SetBuilder.AddBinding(binding, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage);

		Binding bin;
		bin.BufferInfo = m_Buffers[binding]->DescriptorInfo();
		bin.Type = BindingType::Buffer;
		m_Bindings[binding] = bin;
	}

	void Uniform::Build()
	{
		m_DescriptorSetLayout = std::make_shared<DescriptorSetLayout>(m_SetBuilder);

		DescriptorWriter writer(*m_DescriptorSetLayout, m_Pool);
		for (int i = 0; i < (int)m_Bindings.size(); i++)
		{

			switch (m_DescriptorSetLayout->GetDescriptorSetLayoutBindings()[i].descriptorType)
			{

			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
				writer.WriteImage(i, &m_Bindings[i].ImageInfo);
			} break;

			case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
				writer.WriteImage(i, &m_Bindings[i].ImageInfo);
			} break;

			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
				writer.WriteBuffer(i, &m_Bindings[i].BufferInfo);
			} break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
				writer.WriteBuffer(i, &m_Bindings[i].BufferInfo);
			} break;

			default: VL_CORE_ASSERT(false, "Binding Type not supported!"); break;
			}
		}

		writer.Build(m_DescriptorSet);
	}

	void Uniform::Resize(uint32_t binding, uint32_t newSize, VkQueue queue, VkCommandPool pool)
	{
		VL_CORE_ASSERT(binding < m_DescriptorSetLayout->GetDescriptorSetLayoutBindings().size(), "there isn't such binding!");
		VL_CORE_ASSERT(m_Buffers[binding]->GetUsageFlags() & VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "Resizable flag has to be set!");
		VL_CORE_ASSERT(m_DescriptorSetLayout->GetDescriptorSetLayoutBindings()[binding].descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, "Can't resize an image!");
		VL_CORE_ASSERT(m_DescriptorSetLayout->GetDescriptorSetLayoutBindings()[binding].descriptorType != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, "Binding has to be storage buffer!");

		Ref<Buffer> oldBuffer = m_Buffers[binding];
		m_Buffers[binding] = std::make_shared<Buffer>(newSize, 1, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		m_Buffers[binding]->Map();
		m_Bindings[binding].BufferInfo = m_Buffers[binding]->DescriptorInfo();

		// TODO: check whether we actually need to write to all bindings at resize
		DescriptorWriter writer(*m_DescriptorSetLayout, m_Pool);
		for (int i = 0; i < (int)m_DescriptorSetLayout->GetDescriptorSetLayoutBindings().size(); i++) {

			switch (m_DescriptorSetLayout->GetDescriptorSetLayoutBindings()[i].descriptorType) {
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
				writer.WriteImage(i, &m_Bindings[i].ImageInfo);
			} break;

			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
				writer.WriteBuffer(i, &m_Bindings[i].BufferInfo);
			} break;

			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
				writer.WriteBuffer(i, &m_Bindings[i].BufferInfo);
			} break;

			default: VL_CORE_ASSERT(false, "Binding Type not supported!"); break;
			}
		}

		writer.Build(m_DescriptorSet);
		Buffer::CopyBuffer(oldBuffer->GetBuffer(), m_Buffers[binding]->GetBuffer(), oldBuffer->GetBufferSize(), queue, pool);
	}

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