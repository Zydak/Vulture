// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

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

		void Init(CreateInfo createInfo);
		void Destroy();

		PushConstant() = default;
		PushConstant(CreateInfo createInfo);
		~PushConstant();

		PushConstant(const PushConstant& other) = delete;
		PushConstant& operator=(const PushConstant& other) = delete;
		PushConstant(PushConstant&& other) noexcept;
		PushConstant& operator=(PushConstant&& other) noexcept;

		void SetData(const T& data);
		void Push(VkPipelineLayout Layout, VkCommandBuffer cmdBuffer, uint32_t offset = 0);

		inline VkPushConstantRange GetRange() const { return m_Range; }
		inline VkPushConstantRange* GetRangePtr() { return &m_Range; }
		inline T GetData() const { return m_Data; }
		inline T* GetDataPtr() { return &m_Data; }

		inline bool IsInitialized() const { return m_Initialized; }
	private:
		T m_Data = T{};

		VkPushConstantRange m_Range = {};
		VkShaderStageFlags m_Stage = {};
		bool m_Initialized = false;

		void Reset();
	};

	template<typename T>
	PushConstant<T>& PushConstant<T>::operator=(PushConstant<T>&& other) noexcept
	{
		m_Data			= std::move(other.m_Data);
		m_Range			= std::move(other.m_Range);
		m_Stage			= std::move(other.m_Stage);
		m_Initialized	= std::move(other.m_Initialized);

		other.Reset();

		return *this;
	}

	template<typename T>
	PushConstant<T>::PushConstant(PushConstant&& other) noexcept
	{
		m_Data = std::move(other.m_Data);
		m_Range = std::move(other.m_Range);
		m_Stage = std::move(other.m_Stage);
		m_Initialized = std::move(other.m_Initialized);

		other.Reset();
	}

	template<typename T>
	void Vulture::PushConstant<T>::Reset()
	{
		m_Data = T{};

		m_Range = {};
		m_Stage = {};
		m_Initialized = false;
	}

	template<typename T>
	void Vulture::PushConstant<T>::SetData(const T& data)
	{
		m_Data = data;
	}

	template<typename T>
	void Vulture::PushConstant<T>::Destroy()
	{
		if (!m_Initialized)
			Destroy();

		Reset();
	}

	template<typename T>
	void Vulture::PushConstant<T>::Init(CreateInfo createInfo)
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
	Vulture::PushConstant<T>::PushConstant(CreateInfo createInfo)
	{
		Init(createInfo);
	}

}