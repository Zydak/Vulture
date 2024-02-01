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
		Buffer,
		AccelerationStructure,
	};

	struct Binding
	{
		BindingType Type;
		std::vector<VkDescriptorImageInfo> ImageInfo;
		VkDescriptorBufferInfo BufferInfo{};
		VkWriteDescriptorSetAccelerationStructureKHR AccelInfo{};
	};

	class Uniform
	{
	public:
		Uniform(DescriptorPool& pool);
		~Uniform();

		inline Buffer* GetBuffer(const int& binding) { return &m_Buffers[binding]; }

		inline Ref<DescriptorSetLayout> GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }

		inline const VkDescriptorSet& GetDescriptorSet() const { return m_DescriptorSet; }
		inline const DescriptorPool& GetPool() const { return m_Pool; }

		void AddImageSampler(uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout, VkShaderStageFlagBits stage, int count = 1, VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
		void AddAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR asInfo);
		void AddUniformBuffer(uint32_t binding, uint32_t bufferSize, VkShaderStageFlags stage);
		void AddStorageBuffer(uint32_t binding, uint32_t bufferSize, VkShaderStageFlagBits stage, bool resizable = false);
		void Build();
		void Resize(uint32_t binding, uint32_t newSize, VkQueue queue, VkCommandPool pool);
		void UpdateImageSampler(uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout);

		void Bind(const uint32_t& set, const VkPipelineLayout& layout, const VkPipelineBindPoint& bindPoint, const VkCommandBuffer& cmdBuffer);

		uint32_t m_UniformID;
	private:
		DescriptorSetLayout::Builder m_SetBuilder{};

		std::unordered_map<uint32_t, Buffer> m_Buffers;
		Ref<DescriptorSetLayout> m_DescriptorSetLayout;
		VkDescriptorSet m_DescriptorSet;

		std::unordered_map<uint32_t, Binding> m_Bindings;

		DescriptorPool& m_Pool;
	};

}