#pragma once

#include "pch.h"
#include "Vulkan/Device.h"

namespace Vulture
{
	class DescriptorSetLayout
	{
	public:
		struct Binding
		{
			uint32_t BindingNumber = -1;
			uint32_t DescriptorsCount = 1;
			VkDescriptorType Type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
			VkShaderStageFlags StageFlags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

			operator bool() const
			{
				return (BindingNumber != -1) && (Type != VK_DESCRIPTOR_TYPE_MAX_ENUM) && (StageFlags != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
			}
		};

		void Init(std::vector<Binding> bindings);

		DescriptorSetLayout() = default;
		DescriptorSetLayout(const std::vector<Binding>& bindings);
		~DescriptorSetLayout();

		inline VkDescriptorSetLayout GetDescriptorSetLayoutHandle() const { return m_DescriptorSetLayoutHandle; }
		inline std::vector<Binding> GetDescriptorSetLayoutBindings() { return m_Bindings; }

	private:
		VkDescriptorSetLayout m_DescriptorSetLayoutHandle;
		std::vector<Binding> m_Bindings;

		friend class DescriptorWriter;
	};
}