#pragma once
#include "pch.h"

#include "Device.h"

namespace Vulture
{
	class DescriptorSetLayout
	{
	public:
		class Builder
		{
		public:
			Builder() {}

			void AddBinding(uint32_t binding, VkDescriptorType descriptorType, VkShaderStageFlags stageFlags, uint32_t count = 1);
			std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_Bindings{};
		};

		DescriptorSetLayout(DescriptorSetLayout::Builder builder);
		~DescriptorSetLayout();
		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

		inline VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }
		inline std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> GetDescriptorSetLayoutBindings() { return m_Bindings; }

	private:
		VkDescriptorSetLayout m_DescriptorSetLayout;
		std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_Bindings;

		friend class DescriptorWriter;
	};

	class DescriptorPool
	{
	public:
		class Builder
		{
		public:
			Builder() {}

			void AddPoolSize(VkDescriptorType descriptorType, uint32_t count);
			void SetPoolFlags(VkDescriptorPoolCreateFlags flags);
			void SetMaxSets(uint32_t count);

			std::vector<VkDescriptorPoolSize> m_PoolSizes{};
			uint32_t m_MaxSets = 1000;
			VkDescriptorPoolCreateFlags m_PoolFlags = 0;
		private:
			uint32_t m_PoolSize = 0;
		};

		DescriptorPool(const DescriptorPool::Builder& builder);
		~DescriptorPool();
		DescriptorPool(const DescriptorPool&) = delete;
		DescriptorPool& operator=(const DescriptorPool&) = delete;

		bool AllocateDescriptorSets(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;

		inline VkDescriptorPool GetDescriptorPool() { return m_DescriptorPool; }

		void ResetPool();

	private:
		VkDescriptorPool m_DescriptorPool;

		friend class DescriptorWriter;
	};

	class DescriptorWriter
	{
	public:
		DescriptorWriter(DescriptorSetLayout& setLayout, DescriptorPool& pool);

		void WriteBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
		void WriteImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

		bool Build(VkDescriptorSet& set);
		void Overwrite(VkDescriptorSet& set);

	private:
		DescriptorSetLayout& m_SetLayout;
		DescriptorPool& m_Pool;
		std::vector<VkWriteDescriptorSet> m_Writes;
	};

}