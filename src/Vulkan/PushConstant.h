#pragma once

#include "Device.h"

namespace Vulture
{
	template<typename T>
	class PushConstant
	{
	public:
		struct CreateInfo
		{
			VkShaderStageFlags Stage;
		};

		void Init(const CreateInfo& createInfo);
		void Destroy();

		PushConstant() = default;
		PushConstant(const CreateInfo& createInfo);
		~PushConstant();

		void SetData(const T& data);
		void Push(VkPipelineLayout Layout, VkCommandBuffer cmdBuffer, uint32_t offset = 0);

		inline VkPushConstantRange GetRange() const { return m_Range; }
		inline VkPushConstantRange* GetRangePtr() { return &m_Range; }
		inline T GetData() const { return m_Data; }
		inline T* GetDataPtr() { return &m_Data; }
	private:
		T m_Data = T{};

		VkPushConstantRange m_Range;
		VkShaderStageFlags m_Stage;
		bool m_Initialized;
	};

	template<typename T>
	void Vulture::PushConstant<T>::SetData(const T& data)
	{
		m_Data = data;
	}

	template<typename T>
	void Vulture::PushConstant<T>::Destroy()
	{
		m_Initialized = false;
	}

	template<typename T>
	void Vulture::PushConstant<T>::Init(const CreateInfo& createInfo)
	{
		if (m_Initialized)
			Destroy();

		m_Stage = createInfo.Stage;

		m_Range.offset = 0;
		m_Range.size = sizeof(T);
		m_Range.stageFlags = m_Stage;

		m_Initialized = true;
	}

	template<typename T>
	void Vulture::PushConstant<T>::Push(VkPipelineLayout Layout, VkCommandBuffer cmdBuffer, uint32_t offset /*= 0*/)
	{
		vkCmdPushConstants(
			cmdBuffer,
			Layout,
			m_Stage,
			offset,
			sizeof(T),
			&m_Data
		);
	}

	template<typename T>
	Vulture::PushConstant<T>::~PushConstant()
	{
		if (m_Initialized)
			Destroy();
	}

	template<typename T>
	Vulture::PushConstant<T>::PushConstant(const CreateInfo& createInfo)
	{
		Init(createInfo);
	}

}