#pragma once

#include "Vulkan/Device.h"
#include "DescriptorSetLayout.h"
#include "DescriptorPool.h"

namespace Vulture
{
	class DescriptorWriter
	{
	public:
		void Init(DescriptorSetLayout* setLayout, DescriptorPool* pool);

		DescriptorWriter() = default;
		DescriptorWriter(DescriptorSetLayout* setLayout, DescriptorPool* pool);

		void WriteBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
		void WriteAs(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR* asInfo);
		void WriteImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

		bool Build(VkDescriptorSet& set);
		void Overwrite(VkDescriptorSet& set);

	private:
		DescriptorSetLayout* m_SetLayout;
		DescriptorPool* m_Pool;
		std::vector<VkWriteDescriptorSet> m_Writes;
	};
}