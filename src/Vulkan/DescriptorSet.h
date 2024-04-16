#pragma once
#include "pch.h"

#include "Vulkan/Buffer.h"
#include "Descriptors/DescriptorPool.h"
#include "Descriptors/DescriptorSetLayout.h"
#include "Descriptors/DescriptorWriter.h"

namespace Vulture
{
	class Sampler;

	class DescriptorSet
	{
	public:

		void Init(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings, Sampler* samplerForEmptyBindings = nullptr);
		void Destroy();

		DescriptorSet() = default;
		DescriptorSet(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings);
		~DescriptorSet();

		inline const DescriptorSetLayout* GetDescriptorSetLayout() const { return &m_DescriptorSetLayout; }

		inline const VkDescriptorSet& GetDescriptorSetHandle() const { return m_DescriptorSetHandle; }
		inline const DescriptorPool* GetPool() const { return m_Pool; }

		void AddImageSampler(uint32_t binding, VkDescriptorImageInfo info);
		void AddAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR asInfo);
		void AddBuffer(uint32_t binding, VkDescriptorBufferInfo info);
		void Build();
		void UpdateImageSampler(uint32_t binding, VkDescriptorImageInfo info);
		void UpdateBuffer(uint32_t binding, VkDescriptorBufferInfo info);

		void Bind(const uint32_t& set, const VkPipelineLayout& layout, const VkPipelineBindPoint& bindPoint, const VkCommandBuffer& cmdBuffer) const;

	private:

		struct Binding
		{
			VkDescriptorType m_Type;
			uint32_t m_DescriptorCount = 0;
			std::vector<VkDescriptorImageInfo> m_ImageInfo;
			std::vector<VkDescriptorBufferInfo> m_BufferInfo;
			std::vector<VkWriteDescriptorSetAccelerationStructureKHR> m_AccelInfo;
		};

		std::vector<Binding> m_BindingsWriteInfo;
		Vulture::DescriptorSetLayout m_DescriptorSetLayout;
		VkDescriptorSet m_DescriptorSetHandle;

		DescriptorPool* m_Pool;

		bool m_Initialized = false;
	};

}