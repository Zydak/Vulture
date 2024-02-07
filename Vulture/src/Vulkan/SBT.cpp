#include "pch.h"

#include "sbt.h"

namespace Vulture
{
	void SBT::Init(CreateInfo* createInfo)
	{
		if (m_Initialized)
			Destroy();

		VL_CORE_ASSERT(*createInfo, "Incorectly Initialized SBT Create Info!");
		VL_CORE_ASSERT(createInfo->RGenCount == 1, "RGenCount must be 1!");

		m_MissCount = createInfo->MissCount;
		m_HitCount = createInfo->HitCount;
		m_RGenCount = createInfo->RGenCount;
		m_CallableCount = createInfo->CallableCount;
		m_RayTracingPipeline = createInfo->RayTracingPipeline;

		uint32_t handleCount = m_MissCount + m_HitCount + m_RGenCount + m_CallableCount;
		uint32_t handleSize = Vulture::Device::GetRayTracingProperties().shaderGroupHandleSize;

		uint32_t handleSizeAligned = (uint32_t)Vulture::Device::GetAlignment(handleSize, Vulture::Device::GetRayTracingProperties().shaderGroupHandleAlignment);

		m_RgenRegion.stride = Vulture::Device::GetAlignment(handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);
		m_RgenRegion.size = m_RgenRegion.stride;

		m_MissRegion.stride = handleSizeAligned;
		m_MissRegion.size = Vulture::Device::GetAlignment(m_MissCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);

		m_HitRegion.stride = handleSizeAligned;
		m_HitRegion.size = Vulture::Device::GetAlignment(m_HitCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);
		
		m_CallRegion.stride = handleSizeAligned;
		m_CallRegion.size = Vulture::Device::GetAlignment(m_CallableCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);

		// Get the shader group handles
		uint32_t dataSize = handleCount * handleSize;
		std::vector<uint8_t> handles(dataSize);
		auto result = Vulture::Device::vkGetRayTracingShaderGroupHandlesKHR(Vulture::Device::GetDevice(), m_RayTracingPipeline->GetPipeline(), 0, handleCount, dataSize, handles.data());
		VL_CORE_ASSERT(result == VK_SUCCESS, "Failed to get shader group handles!");

		// Allocate a buffer for storing the SBT.
		VkDeviceSize sbtSize = m_RgenRegion.size + m_MissRegion.size + m_HitRegion.size + m_CallRegion.size;

		Vulture::Buffer::CreateInfo bufferInfo{};
		bufferInfo.InstanceSize = sbtSize;
		bufferInfo.InstanceCount = 1;
		bufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
		bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		Vulture::Buffer stagingBuffer;
		stagingBuffer.Init(bufferInfo);

		bufferInfo.InstanceSize = sbtSize;
		bufferInfo.InstanceCount = 1;
		bufferInfo.UsageFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;
		bufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		m_RtSBTBuffer.Init(bufferInfo);

		// Find the SBT addresses of each group
		m_RgenRegion.deviceAddress = m_RtSBTBuffer.GetDeviceAddress();
		m_MissRegion.deviceAddress = m_RtSBTBuffer.GetDeviceAddress() + m_RgenRegion.size;
		m_HitRegion.deviceAddress = m_RtSBTBuffer.GetDeviceAddress() + m_RgenRegion.size + m_MissRegion.size;
		m_CallRegion.deviceAddress = m_RtSBTBuffer.GetDeviceAddress() + m_RgenRegion.size + m_MissRegion.size + m_HitRegion.size;

		auto getHandle = [&](uint32_t i) { return handles.data() + (i * handleSize); };

		stagingBuffer.Map();
		uint8_t* pSBTBuffer = reinterpret_cast<uint8_t*>(stagingBuffer.GetMappedMemory());
		uint8_t* pData = nullptr;
		uint32_t handleIdx = 0;

		// Ray gen
		pData = pSBTBuffer;

		for (uint32_t c = 0; c < m_RGenCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData = pSBTBuffer + m_RgenRegion.stride;
		}

		// Miss
		for (uint32_t c = 0; c < m_MissCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData += m_MissRegion.stride;
		}

		// Hit
		pData = pSBTBuffer + m_RgenRegion.size + m_MissRegion.size;
		for (uint32_t c = 0; c < m_HitCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData += m_HitRegion.stride;
		}

		// Callable
		pData = pSBTBuffer + m_RgenRegion.size + m_MissRegion.size + m_HitRegion.size;
		for (uint32_t c = 0; c < m_CallableCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData += m_CallRegion.stride;
		}

		// Copy the shader binding table to the device local buffer
		Vulture::Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_RtSBTBuffer.GetBuffer(), sbtSize, Vulture::Device::GetGraphicsQueue(), 0, Vulture::Device::GetGraphicsCommandPool());

		stagingBuffer.Unmap();

		m_Initialized = true;
	}

	void SBT::Destroy()
	{
		m_RtSBTBuffer.Destroy();
		m_Initialized = false;
	}

	SBT::~SBT()
	{
		if (m_Initialized)
			Destroy();
	}

	SBT::SBT(CreateInfo* createInfo)
	{
		Init(createInfo);
	}

}