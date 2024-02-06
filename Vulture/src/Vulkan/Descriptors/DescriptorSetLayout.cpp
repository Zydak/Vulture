#include "pch.h"
#include "DescriptorSetLayout.h"

namespace Vulture
{
	// todo: description
	void DescriptorSetLayout::Init(std::vector<Binding> bindings)
	{
		m_Bindings = bindings;
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
		for (int i = 0; i < bindings.size(); i++)
		{
			VL_CORE_ASSERT(bindings[i], "Incorectly initialized binding in array!");

			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.binding = bindings[i].BindingNumber;
			layoutBinding.descriptorType = bindings[i].Type;
			layoutBinding.descriptorCount = bindings[i].DescriptorsCount;
			layoutBinding.stageFlags = bindings[i].StageFlags;

			setLayoutBindings.push_back(layoutBinding);
		}

		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
		descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutInfo.bindingCount = (uint32_t)setLayoutBindings.size();
		descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();

		VL_CORE_RETURN_ASSERT(vkCreateDescriptorSetLayout(Device::GetDevice(), &descriptorSetLayoutInfo, nullptr, &m_DescriptorSetLayoutHandle),
			VK_SUCCESS,
			"failed to create descriptor set layout!"
		);
	}

	DescriptorSetLayout::DescriptorSetLayout(const std::vector<Binding>& bindings)
	{
		Init(bindings);
	}

	DescriptorSetLayout::~DescriptorSetLayout()
	{
		vkDestroyDescriptorSetLayout(Device::GetDevice(), m_DescriptorSetLayoutHandle, nullptr);
	}
}