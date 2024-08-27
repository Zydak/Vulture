// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once
#include "pch.h"

#include "Device.h"
#include <vk_mem_alloc.h>
#include "iostream"

namespace Vulture
{
	class Buffer
	{
	public:
		struct CreateInfo
		{
			VkDeviceSize InstanceSize = 0;
			VkDeviceSize MinOffsetAlignment = 1;
			VkDeviceSize MinMemoryAlignment = 1;
			VkMemoryPropertyFlags MemoryPropertyFlags = 0;
			VkBufferUsageFlags UsageFlags = 0;
			uint64_t InstanceCount = 1;
			bool NoPool = false;

			operator bool() const
			{
				return InstanceSize != 0 && InstanceCount != 0 && UsageFlags != 0 && MemoryPropertyFlags != 0;
			}
		};

		void Init(const Buffer::CreateInfo& createInfo);
		void Destroy();
		Buffer() = default;
		Buffer(const Buffer::CreateInfo& createInfo);
		~Buffer();

		Buffer(const Buffer& other) = delete;
		Buffer& operator=(const Buffer& other) = delete;
		Buffer(Buffer&& other) noexcept;
		Buffer& operator=(Buffer&& other) noexcept;

		VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void Unmap();

		void WriteToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0, VkCommandBuffer cmdBuffer = 0);
		void ReadFromBuffer(void* outData, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

		VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		VkDescriptorBufferInfo DescriptorInfo();
		VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0, VkQueue queue = 0, VkCommandBuffer cmd = 0, VkCommandPool pool = 0);

		operator bool() const
		{
			return m_Initialized;
		}

	public:

		inline VkBuffer GetBuffer() const { return m_BufferHandle; }

		inline void* GetMappedMemory() const { return m_Mapped; }
		VmaAllocationInfo GetMemoryInfo() const;
		VkDeviceAddress GetDeviceAddress() const;
		inline bool IsMapped() const { return m_Mapped; }

		inline uint64_t GetInstanceCount() const { return m_InstanceCount; }
		inline VkDeviceSize GetInstanceSize() const { return m_InstanceSize; }
		inline VkDeviceSize GetAlignmentSize() const { return m_InstanceSize; }
		inline VkDeviceSize GetMinAlignment() const { return m_MinOffsetAlignment; }
		inline VkBufferUsageFlags GetUsageFlags() const { return m_UsageFlags; }
		inline VkMemoryPropertyFlags GetMemoryPropertyFlags() const { return m_MemoryPropertyFlags; }
		inline VkDeviceSize GetBufferSize() const { return m_BufferSize; }
		inline bool GetNoPool() const { return m_NoPool; }
		inline VmaAllocation* GetAllocation() { return m_Allocation; }
		inline VmaPool* GetVmaPool() { return m_Pool; }

		inline bool IsInitialized() const { return m_Initialized; }

	private:
		static VkDeviceSize GetAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

		VkBuffer m_BufferHandle = VK_NULL_HANDLE;
		VmaAllocation* m_Allocation = nullptr;

		VmaPool* m_Pool = nullptr; // only for buffers that are allocated on their own pools
		void* m_Mapped = nullptr;
		VkDeviceSize m_BufferSize = 0;
		VkDeviceSize m_InstanceSize = 0;
		VkDeviceSize m_AlignmentSize = 1;
		VkDeviceSize m_MinOffsetAlignment = 1;
		VkBufferUsageFlags m_UsageFlags = 0;
		VkMemoryPropertyFlags m_MemoryPropertyFlags = 0;

		uint64_t m_InstanceCount = 0;

		bool m_NoPool = false;

		bool m_Initialized = false;

		void Reset();
	};
}
