// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

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

		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

		DescriptorSet(DescriptorSet&&) noexcept;
		DescriptorSet& operator=(DescriptorSet&&) noexcept;

		inline const DescriptorSetLayout* GetDescriptorSetLayout() const { return &m_DescriptorSetLayout; }

		inline const VkDescriptorSet& GetDescriptorSetHandle() const { return m_DescriptorSetHandle; }
		inline const DescriptorPool* GetPool() const { return m_Pool; }

		inline bool IsInitialized() const { return m_Initialized; }

		void AddImageSampler(uint32_t binding, const VkDescriptorImageInfo& info);
		void AddAccelerationStructure(uint32_t binding, const VkWriteDescriptorSetAccelerationStructureKHR& asInfo);
		void AddBuffer(uint32_t binding, const VkDescriptorBufferInfo& info);
		void Build();
		void UpdateImageSampler(uint32_t binding, VkDescriptorImageInfo& info);
		void UpdateBuffer(uint32_t binding, VkDescriptorBufferInfo& info);

		void Bind(uint32_t set, VkPipelineLayout layout, VkPipelineBindPoint bindPoint, VkCommandBuffer cmdBuffer) const;

	private:

		struct Binding
		{
			VkDescriptorType m_Type = {};
			uint32_t m_DescriptorCount = 0;
			std::vector<VkDescriptorImageInfo> m_ImageInfo;
			std::vector<VkDescriptorBufferInfo> m_BufferInfo;
			std::vector<VkWriteDescriptorSetAccelerationStructureKHR> m_AccelInfo;
		};

		std::vector<Binding> m_BindingsWriteInfo;
		Vulture::DescriptorSetLayout m_DescriptorSetLayout;
		VkDescriptorSet m_DescriptorSetHandle = VK_NULL_HANDLE;

		DescriptorPool* m_Pool = nullptr;

		bool m_Initialized = false;

		void Reset();
	};

}