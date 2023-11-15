#include "pch.h"
#include "Utility/Utility.h"

#include "Descriptors.h"

namespace Vulture
{

	// *************** Descriptor Set Layout Builder *********************

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

	void DescriptorPool::Builder::AddPoolSize(VkDescriptorType descriptorType, uint32_t count)
	{
		m_PoolSizes.push_back({ descriptorType, count });
		if (m_PoolSize < count) { m_PoolSize = count; }
	}

	void DescriptorPool::Builder::SetPoolFlags(VkDescriptorPoolCreateFlags flags)
	{
		m_PoolFlags = flags;
	}

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

	bool DescriptorPool::AllocateDescriptorSets(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.pSetLayouts = &descriptorSetLayout;
		allocInfo.descriptorSetCount = 1;

		// Might want to create a "DescriptorPoolManager" class that handles this case, and builds
		// a new pool whenever an old pool fills up.
		if (vkAllocateDescriptorSets(Device::GetDevice(), &allocInfo, &descriptor) != VK_SUCCESS) { return false; }
		return true;
	}

	void DescriptorPool::FreeDescriptors(std::vector<VkDescriptorSet>& descriptors) const
	{
		vkFreeDescriptorSets(Device::GetDevice(), m_DescriptorPool, static_cast<uint32_t>(descriptors.size()), descriptors.data());
	}

	void DescriptorPool::ResetPool() { vkResetDescriptorPool(Device::GetDevice(), m_DescriptorPool, 0); }

	// *************** Descriptor Writer *********************

	DescriptorWriter::DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool) : m_SetLayout{ setLayout }, m_Pool{ pool } {}

	void DescriptorWriter::WriteBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo)
	{
		VL_CORE_ASSERT(m_SetLayout.m_Bindings.count(binding) == 1, "Layout does not contain specified binding");

		auto& bindingDescription = m_SetLayout.m_Bindings[binding];

		VL_CORE_ASSERT(bindingDescription.descriptorCount == 1, "Binding single descriptor info, but binding expects multiple");

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = bindingDescription.descriptorType;
		write.dstBinding = binding;
		write.pBufferInfo = bufferInfo;
		write.descriptorCount = 1;

		m_Writes.push_back(write);
	}

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

	bool DescriptorWriter::Build(VkDescriptorSet& set)
	{
		// Might want to create a "DescriptorPoolManager" class that handles this case, and builds
		// a new pool whenever an old pool fills up.
		VL_CORE_RETURN_ASSERT(m_Pool.AllocateDescriptorSets(m_SetLayout.GetDescriptorSetLayout(), set), 
			true, 
			"Failed to build descriptor"
		);
		Overwrite(set);
		return true;
	}

	void DescriptorWriter::Overwrite(VkDescriptorSet& set)
	{
		for (auto& write : m_Writes) { write.dstSet = set; }
		vkUpdateDescriptorSets(Device::GetDevice(), (uint32_t)m_Writes.size(), m_Writes.data(), 0, nullptr);
		m_Writes.clear();
	}

}