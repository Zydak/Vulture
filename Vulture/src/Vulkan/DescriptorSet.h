#pragma once
#include "pch.h"

#include "Vulkan/Buffer.h"
#include "Descriptors/DescriptorPool.h"
#include "Descriptors/DescriptorSetLayout.h"
#include "Descriptors/DescriptorWriter.h"

namespace Vulture
{
	class DescriptorSet
	{
	public:

		void Init(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings);

		DescriptorSet() = default;
		DescriptorSet(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings);
		~DescriptorSet();

		inline Buffer* GetBuffer(int binding, int buffer = 0) { return &m_Buffers[binding][buffer]; }
		// Write to buffer

		inline const DescriptorSetLayout* GetDescriptorSetLayout() const { return &m_DescriptorSetLayout; }

		inline const VkDescriptorSet& GetDescriptorSet() const { return m_DescriptorSet; }
		inline const DescriptorPool* GetPool() const { return m_Pool; }

		void AddImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
		void AddAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR asInfo);
		void AddUniformBuffer(uint32_t binding, uint32_t bufferSize);
		void AddStorageBuffer(uint32_t binding, uint32_t bufferSize, bool resizable = false);
		void Build();
		void Resize(uint32_t binding, uint32_t newSize, VkQueue queue, VkCommandPool pool, uint32_t buffer = 0);
		void UpdateImageSampler(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout);

		void Bind(const uint32_t& set, const VkPipelineLayout& layout, const VkPipelineBindPoint& bindPoint, const VkCommandBuffer& cmdBuffer);

	private:

		enum BindingType
		{
			Buffer,
			Image,
			AS
		};

		struct Binding
		{
			BindingType Type;
			std::vector<VkDescriptorImageInfo> ImageInfo;
			std::vector<VkDescriptorBufferInfo> BufferInfo;
			std::vector<VkWriteDescriptorSetAccelerationStructureKHR> AccelInfo;
		};

		std::unordered_map<uint32_t, std::vector<Vulture::Buffer>> m_Buffers;
		std::unordered_map<uint32_t, Binding> m_BindingsWriteInfo;
		DescriptorSetLayout m_DescriptorSetLayout;
		VkDescriptorSet m_DescriptorSet;

		DescriptorPool* m_Pool;
	};

}