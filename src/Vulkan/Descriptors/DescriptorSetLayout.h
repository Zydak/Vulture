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
			int BindingNumber = -1;
			uint32_t DescriptorsCount = 1;
			VkDescriptorType Type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
			VkShaderStageFlags StageFlags = VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;

			operator bool() const
			{
				return (BindingNumber != -1) && (Type != VK_DESCRIPTOR_TYPE_MAX_ENUM) && (StageFlags != VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM);
			}
		};

		void Init(const std::vector<Binding>& bindings);
		void Destroy();

		DescriptorSetLayout() = default;
		DescriptorSetLayout(const std::vector<Binding>& bindings);
		~DescriptorSetLayout();

		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

		DescriptorSetLayout(DescriptorSetLayout&&) noexcept;
		DescriptorSetLayout& operator=(DescriptorSetLayout&&) noexcept;

		inline VkDescriptorSetLayout GetDescriptorSetLayoutHandle() const { return m_DescriptorSetLayoutHandle; }
		inline std::vector<Binding> GetDescriptorSetLayoutBindings() { return m_Bindings; }

		inline bool IsInitialized() const { return m_Initialized; }

	private:
		VkDescriptorSetLayout m_DescriptorSetLayoutHandle = VK_NULL_HANDLE;
		std::vector<Binding> m_Bindings;

		bool m_Initialized = false;

		void Reset();

		friend class DescriptorWriter;
	};
}