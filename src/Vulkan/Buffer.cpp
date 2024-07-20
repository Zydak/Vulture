#include "pch.h"
#include "Utility/Utility.h"

#include "Buffer.h"
#include "DeleteQueue.h"

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
	 * @param instanceSize - Size of an instance
	 * @param minOffsetAlignment - Minimum required alignment in bytes
	 */
	VkDeviceSize Buffer::GetAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment)
	{
		if (minOffsetAlignment > 0) { return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1); }
		return instanceSize;
	}

	void Buffer::Reset()
	{
		m_Mapped = nullptr;
		m_BufferHandle = VK_NULL_HANDLE;
		m_Allocation = nullptr;
		m_Pool = nullptr; // only for buffers that are allocated on their own pools

		m_BufferSize = 0;
		m_InstanceCount = 0;
		m_InstanceSize = 0;
		m_AlignmentSize = 1;
		m_UsageFlags = 0;
		m_MemoryPropertyFlags = 0;
		m_MinOffsetAlignment = 1; // Stored only for copies of the buffer
		m_NoPool = false;

		m_Initialized = false;
	}

	/**
	 * @brief Initializes the buffer with the specified creation information.
	 *
	 * @param createInfo - creation information for the buffer.
	 */
	void Buffer::Init(const Buffer::CreateInfo& createInfo)
	{
		// Check if the buffer has already been initialized.
		if (m_Initialized)
			Destroy(); // If already initialized, destroy the existing buffer.

		m_Allocation = new VmaAllocation();

		// Assert the validity of the provided creation information.
		VL_CORE_ASSERT(createInfo, "Incorrectly Initialized Buffer::CreateInfo! Values: InstanceCount: {0}, InstanceSize: {1}, UsageFlags: {2}, MemoryPropertyFlags: {3}", createInfo.InstanceCount, createInfo.InstanceSize, createInfo.UsageFlags, createInfo.MemoryPropertyFlags);

		// Mark the buffer as initialized.
		m_Initialized = true;

		// Store the creation information.
		m_InstanceCount = createInfo.InstanceCount;
		m_UsageFlags = createInfo.UsageFlags;
		m_MemoryPropertyFlags = createInfo.MemoryPropertyFlags;
		m_InstanceSize = createInfo.InstanceSize;
		m_NoPool = createInfo.NoPool;
		if (m_NoPool)
			m_Pool = new VmaPool();
		m_MinOffsetAlignment = createInfo.MinOffsetAlignment;

		// Calculate alignment size and total buffer size.
		m_AlignmentSize = GetAlignment(m_InstanceSize, createInfo.MinOffsetAlignment);
		m_BufferSize = m_AlignmentSize * m_InstanceCount;

		// Prepare the buffer creation information.
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = m_BufferSize;
		bufferInfo.usage = m_UsageFlags;

		// Just like the images in the swap chain, buffers can also be owned
		// by a specific queue family or be shared between multiple at the same time.
		// The buffer will only be used from the graphics queue, so we can stick to exclusive access.
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		// Create the Vulkan buffer and allocate memory for it.
		Device::CreateBuffer(bufferInfo, m_BufferHandle, *m_Allocation, m_MemoryPropertyFlags, &*m_Pool, m_NoPool, createInfo.MinMemoryAlignment);
	}

	/**
	 * @brief Destroys the Vulkan buffer associated with the Buffer object,
	 * unmaps any mapped memory, deallocates the buffer memory, and marks the buffer as uninitialized.
	 *
	 * @note: It is the responsibility of the caller to ensure that the buffer is not in use before calling this function.
	 */
	void Buffer::Destroy()
	{
		if (!m_Initialized)
			return;

		// Check if the buffer is initialized.
		VL_CORE_ASSERT(m_Initialized, "Can't destroy buffer that is not initialized!");

		// Unmap the buffer memory if it was mapped.
		Unmap();

		DeleteQueue::TrashBuffer(*this);

		m_Pool = nullptr;

		Reset();
	}

	/**
	 * @brief Copies data from the source buffer to the destination buffer.
	 *
	 * @param srcBuffer - Source buffer from which data will be copied.
	 * @param dstBuffer - Destination buffer to which data will be copied.
	 * @param size - Size in bytes of the data to be copied.
	 * @param queue - Vulkan queue where the command buffer will be submitted.
	 * @param srcOffset - Offset in bytes in src buffer.
	 * @param dstOffset - Offset in bytes in dst buffer.
	 * @param cmd (Optional) - Command buffer to use for the copy operation. If not provided, a temporary command buffer will be created.
	 * @param pool (Optional) - Command pool from which to allocate the command buffer if one is not provided.
	 */
	void Buffer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset, VkDeviceSize dstOffset, VkQueue queue, VkCommandBuffer cmd, VkCommandPool pool)
	{
		bool hasCmd = cmd != VK_NULL_HANDLE; // Check if a command buffer is provided.

		// If no command buffer is provided, begin a temporary single time command buffer.
		if (!hasCmd)
		{
			Device::BeginSingleTimeCommands(cmd, pool);
		}

		// Define the region to copy.
		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = srcOffset;   // Optional
		copyRegion.dstOffset = dstOffset;   // Optional
		copyRegion.size = size;

		if (size == VK_WHOLE_SIZE)
		{
			VL_CORE_ASSERT(false, "");
		}

		// Copy data from the source buffer to the destination buffer.
		vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);

		// If no command buffer is provided, end the temporary single time command buffer and submit it.
		if (!hasCmd)
			Device::EndSingleTimeCommands(cmd, queue, pool);
	}

	/**
	 * @brief Retrieves memory allocation information for the buffer.
	 *
	 * @return - VmaAllocationInfo structure containing memory allocation information.
	 */
	VmaAllocationInfo Buffer::GetMemoryInfo() const
	{
		// Check if the Buffer has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Buffer Not Initialized!");

		// Retrieve memory allocation information from VMA.
		VmaAllocationInfo info{};
		vmaGetAllocationInfo(Device::GetAllocator(), *m_Allocation, &info);

		// Return the memory allocation information.
		return info;
	}

	/**
	 * @brief Retrieves the device address of the buffer.
	 *
	 * @return The device address of the buffer.
	 */
	VkDeviceAddress Buffer::GetDeviceAddress() const
	{
		// Check if the Buffer has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Buffer Not Initialized!");

		// Prepare the buffer device address info.
		VkBufferDeviceAddressInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		info.buffer = m_BufferHandle;

		// Retrieve the device address of the buffer.
		return vkGetBufferDeviceAddress(Device::GetDevice(), &info);
	}

	/**
	 * @brief Constructor for the Buffer class.
	 *
	 * @param createInfo - Creation information for the buffer.
	 */
	Buffer::Buffer(const Buffer::CreateInfo& createInfo)
	{
		// Initialize the Buffer with the provided creation information.
		Init(createInfo);
	}

	Buffer::Buffer(Buffer&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Mapped = std::move(other.m_Mapped);
		m_BufferHandle = std::move(other.m_BufferHandle);
		m_Allocation = std::move(other.m_Allocation);
		m_Pool = std::move(other.m_Pool); // only for buffers that are allocated on their own pools

		m_BufferSize = std::move(other.m_BufferSize);
		m_InstanceCount = std::move(other.m_InstanceCount);
		m_InstanceSize = std::move(other.m_InstanceSize);
		m_AlignmentSize = std::move(other.m_AlignmentSize);
		m_UsageFlags = std::move(other.m_UsageFlags);
		m_MemoryPropertyFlags = std::move(other.m_MemoryPropertyFlags);
		m_MinOffsetAlignment = std::move(other.m_MinOffsetAlignment); // Stored only for copies of the buffer
		m_NoPool = std::move(other.m_NoPool);

		m_Initialized = other.m_Initialized;

		other.Reset();
	}

	Buffer& Buffer::operator=(Buffer&& other) noexcept
	{
		if (m_Initialized)
			Destroy();

		m_Mapped = std::move(other.m_Mapped);
		m_BufferHandle = std::move(other.m_BufferHandle);
		m_Allocation = std::move(other.m_Allocation);
		m_Pool = std::move(other.m_Pool); // only for buffers that are allocated on their own pools

		m_BufferSize = std::move(other.m_BufferSize);
		m_InstanceCount = std::move(other.m_InstanceCount);
		m_InstanceSize = std::move(other.m_InstanceSize);
		m_AlignmentSize = std::move(other.m_AlignmentSize);
		m_UsageFlags = std::move(other.m_UsageFlags);
		m_MemoryPropertyFlags = std::move(other.m_MemoryPropertyFlags);
		m_MinOffsetAlignment = std::move(other.m_MinOffsetAlignment); // Stored only for copies of the buffer
		m_NoPool = std::move(other.m_NoPool);

		m_Initialized = other.m_Initialized;

		other.Reset();

		return *this;
	}

	/**
	 * @brief Destructor for the Buffer class.
	 */
	Buffer::~Buffer()
	{
		Destroy();
	}

	/**
	 * Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
	 *
	 * @param size (Optional) - Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete
	 * buffer range.
	 * @param offset (Optional) - Byte offset from beginning
	 */
	VkResult Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
	{
		// Check if the Buffer has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Buffer Not Initialized!");

		// Check if the buffer is not device local, as device local buffers cannot be mapped.
		VL_CORE_ASSERT(!(m_MemoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT), "Can't map device local buffer!");

		// Map the memory range of the buffer into CPU accessible memory.
		VkResult result = vmaMapMemory(Device::GetAllocator(), *m_Allocation, &m_Mapped);

		return result;
	}

	/**
	 * @brief Unmaps a previously mapped memory range of the buffer.
	 */
	void Buffer::Unmap()
	{
		// Check if the Buffer has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Buffer Not Initialized!");

		// If the buffer is mapped, unmap the memory range.
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
	 * @param size (Optional) - Size of the data to copy. Pass VK_WHOLE_SIZE to flush the complete buffer
	 * range.
	 * @param offset (Optional) - Byte offset from beginning of mapped region.
	 * @param cmdBuffer (Optional) - Vulkan Command Buffer.
	 *
	 */
	void Buffer::WriteToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset, VkCommandBuffer cmdBuffer)
	{
		// Check if the Buffer has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Buffer Not Initialized!");

		// Check if the data size is valid.
		VL_CORE_ASSERT((size == VK_WHOLE_SIZE || size <= m_BufferSize), "Data size is larger than buffer size, either resize the buffer or create a larger one");

		// Check if the data pointer is valid.
		VL_CORE_ASSERT(data != nullptr, "Invalid data pointer");

		if (size == VK_WHOLE_SIZE)
			size = m_BufferSize;

		// If no command buffer is provided, begin a temporary single time command buffer.
		VkCommandBuffer cmd;
		if (cmdBuffer == VK_NULL_HANDLE)
			Device::BeginSingleTimeCommands(cmd, Device::GetGraphicsCommandPool());
		else
			cmd = cmdBuffer;

		// If the buffer is device local, use a staging buffer to transfer the data.
		if (m_MemoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		{
			// Create a staging buffer.
			Buffer::CreateInfo info{};
			info.InstanceCount = 1;
			info.InstanceSize = size == VK_WHOLE_SIZE ? m_BufferSize : size;
			info.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			info.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			Buffer stagingBuffer(info);

			// Map the staging buffer.
			stagingBuffer.Map();

			// Write data to the staging buffer.
			stagingBuffer.WriteToBuffer(data, size, 0, cmd);

			// Unmap the staging buffer.
			stagingBuffer.Unmap();

			// Copy data from the staging buffer to the device local buffer.
			Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_BufferHandle, size, 0, offset, Device::GetGraphicsQueue(), cmd, Device::GetGraphicsCommandPool());
		}
		else // If the buffer is not device local, write directly to the buffer.
		{
			// Check if the buffer is mapped.
			VL_CORE_ASSERT(m_Mapped, "Cannot copy to unmapped buffer");

			// Calculate the memory offset within the mapped buffer.
			char* memOffset = reinterpret_cast<char*>(m_Mapped) + offset;

			// Copy data to the buffer.
			if (size == VK_WHOLE_SIZE)
			{
				memcpy(m_Mapped, data, m_BufferSize);
			}
			else
			{
				memcpy(memOffset, data, size);
			}
		}

		// If no command buffer was provided, end the temporary single time command buffer and submit it.
		if (cmdBuffer == VK_NULL_HANDLE)
			Device::EndSingleTimeCommands(cmd, Device::GetGraphicsQueue(), Device::GetGraphicsCommandPool());
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
		// Check if the Buffer has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Buffer Not Initialized!");

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
		// Check if the Buffer has been initialized.
		VL_CORE_ASSERT(m_Initialized, "Buffer Not Initialized!");

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
	VkDescriptorBufferInfo Buffer::DescriptorInfo()
	{
		return VkDescriptorBufferInfo
		{
			m_BufferHandle,
			0,
			m_BufferSize,
		};
	}

}