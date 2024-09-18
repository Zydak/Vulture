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
		void Destroy();

		DescriptorWriter() = default;
		DescriptorWriter(DescriptorSetLayout* setLayout, DescriptorPool* pool);
		~DescriptorWriter();

		DescriptorWriter(const DescriptorWriter&) = delete;
		DescriptorWriter& operator=(const DescriptorWriter&) = delete;

		DescriptorWriter(DescriptorWriter&&) noexcept;
		DescriptorWriter& operator=(DescriptorWriter&&) noexcept;

		void WriteBuffer(uint32_t binding, const VkDescriptorBufferInfo* bufferInfo);
		void WriteAs(uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR* asInfo);
		void WriteImage(uint32_t binding, const VkDescriptorImageInfo* imageInfo);

		bool Build(VkDescriptorSet* set, bool allocateNewSet = true);
		void Overwrite(VkDescriptorSet* set);

		inline bool IsInitialized() const { return m_Initialized; }

	private:
		DescriptorSetLayout* m_SetLayout;
		DescriptorPool* m_Pool;
		std::vector<VkWriteDescriptorSet> m_Writes;

		bool m_Initialized = false;

		void Reset();
	};
}