#include "pch.h"
#include "Utility/Utility.h"

#include "Buffer.h"

namespace Vulture
{

	/* VULKAN MEMORY TYPES
	 *  Device-Local Memory:
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT:
				Device-local memory is memory that is optimized for the GPU.
				It's usually not directly accessible by the CPU. Data stored in device-local memory can be accessed very efficiently by
				the GPU, making it suitable for resources that don't need frequent CPU interaction, such as large textures and
				buffers used for rendering.

		Host-Visible Memory:
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT:
				Host-visible memory can be accessed directly by the CPU. Changes made to data in this
				memory can be seen by both the CPU and the GPU. However, this type of memory
				might not be as efficient for GPU access as device-local memory. (Synchronization required)

			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
				When this property is specified, changes made by the CPU to this memory
				are immediately visible to the GPU without the need for explicit synchronization.

			VK_MEMORY_PROPERTY_HOST_CACHED_BIT:
				Memory with this property indicates that the CPU cache should be used
				for reads and writes, optimizing access from the CPU side. However, the CPU changes might not be immediately
				visible to the GPU, so synchronization is required.

		Host-Coherent, Host-Visible Memory:
			A combination of VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			provides memory that is both directly accessible by the CPU and automatically synchronized between CPU and GPU.

		Lazy Host-Visible Memory:
			Memory types without VK_MEMORY_PROPERTY_HOST_COHERENT_BIT might require explicit flushing and
			invalidating mechanisms to ensure data consistency between the CPU and GPU.

		Cached Host-Visible Memory:
			Memory types with VK_MEMORY_PROPERTY_HOST_CACHED_BIT enable CPU caching for improved performance when
			accessing memory from the CPU. However, you need to manage data synchronization between CPU and GPU explicitly.

		Device-Local Coherent and Cached Memory:
			Some memory types can also have both VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT and VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			or VK_MEMORY_PROPERTY_HOST_CACHED_BIT, providing coherent or cached access from both the CPU and the GPU.
	*/

	/**
	 * Returns the minimum instance size required to be compatible with devices minOffsetAlignment
	 *
	 * @param instanceSize The size of an instance
	 * @param minOffsetAlignment The minimum required alignment, in bytes, for the offset member (eg
	 * minUniformBufferOffsetAlignment)
	 */
	VkDeviceSize Buffer::GetAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment)
	{
		if (minOffsetAlignment > 0) { return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1); }
		return instanceSize;
	}

	Buffer::Buffer(VkDeviceSize instanceSize, uint32_t instanceCount, VkBufferUsageFlags usageFlags,
		VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize minOffsetAlignment
	)
		: m_InstanceSize(instanceSize), m_InstanceCount(instanceCount), m_UsageFlags(usageFlags),
		m_MemoryPropertyFlags(memoryPropertyFlags)
	{
		m_Allocation = new VmaAllocation();
		m_AlignmentSize = GetAlignment(instanceSize, minOffsetAlignment);
		m_BufferSize = m_AlignmentSize * m_InstanceCount;

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = m_BufferSize;
		bufferInfo.usage = usageFlags;
		
		// Just like the images in the swap chain, buffers can also be owned
		// by a specific queue family or be shared between multiple at the same time.
		// The buffer will only be used from the graphics queue, so we can stick to exclusive access.
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		Device::CreateBuffer(bufferInfo, m_Buffer, *m_Allocation, memoryPropertyFlags);
	}

	Buffer::~Buffer()
	{
		Unmap();
		vmaDestroyBuffer(Device::GetAllocator(), m_Buffer, *m_Allocation);
		delete m_Allocation;
	}

	void Buffer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkQueue queue, VkCommandPool pool)
	{
		VkCommandBuffer commandBuffer;
		Device::BeginSingleTimeCommands(commandBuffer, pool);

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;    // Optional
		copyRegion.dstOffset = 0;    // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		Device::EndSingleTimeCommands(commandBuffer, queue, pool);
	}

	VkDeviceAddress Buffer::GetDeviceAddress() const
	{
		VkBufferDeviceAddressInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		info.buffer = m_Buffer;

		return vkGetBufferDeviceAddress(Device::GetDevice(), &info);
	}

	/**
	 * Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
	 *
	 * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete
	 * buffer range.
	 * @param offset (Optional) Byte offset from beginning
	 */
	VkResult Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
	{
		return vmaMapMemory(Device::GetAllocator(), *m_Allocation, &m_Mapped);
	}

	/**
	 * Unmap a mapped memory range
	 */
	void Buffer::Unmap()
	{
		if (m_Mapped)
		{
			vmaUnmapMemory(Device::GetAllocator(), *m_Allocation);
			m_Mapped = nullptr;
		}
	}

	/**
	 * Copies the specified data to the mapped buffer. Default value writes whole buffer range
	 *
	 * @param data Pointer to the data to copy
	 * @param size (Optional) Size of the data to copy. Pass VK_WHOLE_SIZE to flush the complete buffer
	 * range.
	 * @param offset (Optional) Byte offset from beginning of mapped region
	 *
	 */
	void Buffer::WriteToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset)
	{
		VL_CORE_ASSERT(m_Mapped, "Cannot copy to unmapped buffer");
		VL_CORE_ASSERT(data != nullptr, "invalid data");

		if (size == VK_WHOLE_SIZE) { memcpy(m_Mapped, data, m_BufferSize); }
		else
		{
			char* memOffset = (char*)m_Mapped;
			memOffset += offset;
			memcpy(memOffset, data, size);
		}
	}

	/**
	 * Copies the specified data to the buffer.
	 *
	 * @param data - Pointer to the data to copy
	 * @param size - Size of the data to copy. Pass VK_WHOLE_SIZE to flush the complete buffer
	 * range.
	 * @param offset - Byte offset from beginning of mapped region
	 *
	 */
	void Buffer::WriteToBuffer(VkCommandBuffer cmdBuffer, void* data, VkDeviceSize size, VkDeviceSize offset)
	{
		vkCmdUpdateBuffer(cmdBuffer, m_Buffer, offset, size, data);
	}

	/**
	 * When you modify memory that has been mapped using vkMapMemory, the changes are not immediately visible to the GPU.
	 * To ensure the GPU sees these changes, you use vkFlushMappedMemoryRanges. This function takes an array of memory ranges as input
	 * and flushes (synchronizes) the changes made to those ranges from the CPU side to the GPU side. This essentially
	 * informs the Vulkan implementation that the CPU is done with its writes and that the GPU should see the updated data.
	 *
	 * @note Only required for non-coherent memory
	 *
	 * @param size (Optional) - Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the
	 * complete buffer range.
	 * @param offset (Optional) - Byte offset from beginning
	 *
	 * @return VkResult of the flush call
	 */
	VkResult Buffer::Flush(VkDeviceSize size, VkDeviceSize offset)
	{
		return vmaFlushAllocation(Device::GetAllocator(), *m_Allocation, offset, size);;
	}

	/**
	 * @brief a memory range of the buffer to make it visible to the host
	 *
	 * @note Only required for non-coherent memory
	 *
	 * @param size (Optional) - Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate
	 * the complete buffer range.
	 * @param offset (Optional) - Byte offset from beginning
	 *
	 * @return VkResult of the invalidate call
	 */
	VkResult Buffer::Invalidate(VkDeviceSize size, VkDeviceSize offset)
	{
		return vmaInvalidateAllocation(Device::GetAllocator(), *m_Allocation, offset, size);
	}

	/**
	 * Create a buffer info descriptor
	 *
	 * @param size (Optional) - Size of the memory range of the descriptor
	 * @param offset (Optional) - Byte offset from beginning
	 *
	 * @return VkDescriptorBufferInfo of specified offset and range
	 */
	VkDescriptorBufferInfo Buffer::DescriptorInfo(VkDeviceSize size, VkDeviceSize offset)
	{
		return VkDescriptorBufferInfo
		{
			m_Buffer,
			offset,
			size,
		};
	}

}