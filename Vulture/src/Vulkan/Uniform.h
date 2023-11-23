#pragma once
#include "pch.h"

#include "Buffer.h"
#include "Descriptors.h"
#include "Device.h"
#include "Utility/Utility.h"

namespace Vulture
{

	enum class BindingType
	{
		Image,
		Buffer
	};

	struct Binding
	{
		BindingType Type;
		VkDescriptorImageInfo ImageInfo{};
		VkDescriptorBufferInfo BufferInfo{};
	};

	class Uniform
	{
	public:
		Uniform(DescriptorPool& pool);
		~Uniform();

		inline Ref<Buffer> GetBuffer(const int& binding) { return m_Buffers[binding]; }

		inline Ref<DescriptorSetLayout> GetDescriptorSetLayout() { return m_DescriptorSetLayout; }

		inline VkDescriptorSet& GetDescriptorSet() { return m_DescriptorSet; }
		inline DescriptorPool& GetPool() { return m_Pool; }

		void AddImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout, VkShaderStageFlagBits stage, VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		void AddUniformBuffer(uint32_t binding, uint32_t bufferSize, VkShaderStageFlagBits stage);
		void AddStorageBuffer(uint32_t binding, uint32_t bufferSize, VkShaderStageFlagBits stage, bool resizable = false);
		void Build();
		void Resize(uint32_t binding, uint32_t newSize, VkQueue queue, VkCommandPool pool);
		void UpdateImageSampler(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout);

		void Bind(const uint32_t& set, const VkPipelineLayout& layout, const VkPipelineBindPoint& bindPoint, const VkCommandBuffer& cmdBuffer);

		uint32_t m_UniformID;
	private:
		DescriptorSetLayout::Builder m_SetBuilder{};

		std::unordered_map<uint32_t, Ref<Buffer>> m_Buffers;
		Ref<DescriptorSetLayout> m_DescriptorSetLayout;
		VkDescriptorSet m_DescriptorSet;

		std::unordered_map<uint32_t, Binding> m_Bindings;

		DescriptorPool& m_Pool;
	};

}