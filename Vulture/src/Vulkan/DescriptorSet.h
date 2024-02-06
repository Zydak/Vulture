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
		void Destroy();

		DescriptorSet() = default;
		DescriptorSet(DescriptorPool* pool, const std::vector<DescriptorSetLayout::Binding>& bindings);
		~DescriptorSet();

		inline Buffer* GetBuffer(int binding, int buffer = 0) { return &m_Buffers[binding][buffer]; }
		inline void WriteToBuffer(int binding, int buffer = 0);

		inline const DescriptorSetLayout* GetDescriptorSetLayout() const { return &m_DescriptorSetLayout; }

		inline const VkDescriptorSet& GetDescriptorSetHandle() const { return m_DescriptorSetHandle; }
		inline const DescriptorPool* GetPool() const { return m_Pool; }

		void AddImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
		void AddAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR asInfo);
		void AddUniformBuffer(uint32_t binding, uint32_t bufferSize);
		void AddStorageBuffer(uint32_t binding, uint32_t bufferSize, bool resizable = false);
		void Build();
		void Resize(uint32_t binding, uint32_t newSize, VkQueue queue, VkCommandPool pool, uint32_t buffer = 0);
		void UpdateImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout layout);

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
			BindingType m_Type;
			std::vector<VkDescriptorImageInfo> m_ImageInfo;
			std::vector<VkDescriptorBufferInfo> m_BufferInfo;
			std::vector<VkWriteDescriptorSetAccelerationStructureKHR> m_AccelInfo;
		};

		std::unordered_map<uint32_t, std::vector<Vulture::Buffer>> m_Buffers;
		std::unordered_map<uint32_t, Binding> m_BindingsWriteInfo;
		Vulture::DescriptorSetLayout m_DescriptorSetLayout;
		VkDescriptorSet m_DescriptorSetHandle;

		DescriptorPool* m_Pool;

		bool m_Initialized = false;
	};

}