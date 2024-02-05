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
			uint32_t InstanceCount = 0;
			VkBufferUsageFlags UsageFlags = 0;
			VkMemoryPropertyFlags MemoryPropertyFlags = 0;
			VkDeviceSize MinOffsetAlignment = 1;
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

		Buffer(const Buffer& other);
		Buffer& operator=(const Buffer& other);

		VkResult Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void Unmap();

		void WriteToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		void WriteToBuffer(VkCommandBuffer cmdBuffer, void* data, VkDeviceSize size, VkDeviceSize offset);
		VkResult Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		VkDescriptorBufferInfo DescriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		VkResult Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
		static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkQueue queue, VkCommandBuffer cmd, VkCommandPool pool = 0);

		operator bool() const
		{
			return m_Info.Initialized;
		}

	public:
		struct Info
		{
			void* Mapped = nullptr;
			VkBuffer Buffer = VK_NULL_HANDLE;
			VmaAllocation* Allocation = nullptr;

			VkDeviceSize BufferSize = 0;
			uint32_t InstanceCount = 0;
			VkDeviceSize InstanceSize = 0;
			VkDeviceSize AlignmentSize = 1;
			VkBufferUsageFlags UsageFlags = 0;
			VkMemoryPropertyFlags MemoryPropertyFlags = 0;
			VkDeviceSize MinOffsetAlignment = 1; // Stored only for copies of the buffer
			bool NoPool = false;

			bool Initialized = false;
		};

		inline Buffer::Info GetInfo() const { return m_Info; }
		inline VkBuffer GetBuffer() const { return m_Info.Buffer; }

		inline void* GetMappedMemory() const { return m_Info.Mapped; }
		VmaAllocationInfo GetMemoryInfo() const;
		VkDeviceAddress GetDeviceAddress() const;
		inline bool IsMapped() const { return m_Info.Mapped; }

		inline uint32_t GetInstanceCount() const { return m_Info.InstanceCount; }
		inline VkDeviceSize GetInstanceSize() const { return m_Info.InstanceSize; }
		inline VkDeviceSize GetAlignmentSize() const { return m_Info.InstanceSize; }
		inline VkDeviceSize GetMinAlignment() const { return m_Info.MinOffsetAlignment; }
		inline VkBufferUsageFlags GetUsageFlags() const { return m_Info.UsageFlags; }
		inline VkMemoryPropertyFlags GetMemoryPropertyFlags() const { return m_Info.MemoryPropertyFlags; }
		inline VkDeviceSize GetBufferSize() const { return m_Info.BufferSize; }
		inline bool GetNoPool() const { return m_Info.NoPool; }

	private:
		static VkDeviceSize GetAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

		Buffer::Info m_Info{};
	};
}
