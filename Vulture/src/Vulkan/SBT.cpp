#include "pch.h"

#include "sbt.h"

namespace Vulture
{
	void SBT::Init(CreateInfo* createInfo)
	{
		if (m_Info.Initialized)
			Destroy();

		VL_CORE_ASSERT(*createInfo, "Incorectly Initialized SBT Create Info!");
		VL_CORE_ASSERT(createInfo->RGenCount == 1, "RGenCount must be 1!");

		m_Info.MissCount = createInfo->MissCount;
		m_Info.HitCount = createInfo->HitCount;
		m_Info.RGenCount = createInfo->RGenCount;
		m_Info.CallableCount = createInfo->CallableCount;
		m_Info.RayTracingPipeline = createInfo->RayTracingPipeline;

		uint32_t handleCount = m_Info.MissCount + m_Info.HitCount + m_Info.RGenCount + m_Info.CallableCount;
		uint32_t handleSize = Vulture::Device::GetRayTracingProperties().shaderGroupHandleSize;

		uint32_t handleSizeAligned = (uint32_t)Vulture::Device::GetAlignment(handleSize, Vulture::Device::GetRayTracingProperties().shaderGroupHandleAlignment);

		m_Info.RgenRegion.stride = Vulture::Device::GetAlignment(handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);
		m_Info.RgenRegion.size = m_Info.RgenRegion.stride;

		m_Info.MissRegion.stride = handleSizeAligned;
		m_Info.MissRegion.size = Vulture::Device::GetAlignment(m_Info.MissCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);

		m_Info.HitRegion.stride = handleSizeAligned;
		m_Info.HitRegion.size = Vulture::Device::GetAlignment(m_Info.HitCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);
		
		m_Info.CallRegion.stride = handleSizeAligned;
		m_Info.CallRegion.size = Vulture::Device::GetAlignment(m_Info.CallableCount * handleSizeAligned, Vulture::Device::GetRayTracingProperties().shaderGroupBaseAlignment);

		// Get the shader group handles
		uint32_t dataSize = handleCount * handleSize;
		std::vector<uint8_t> handles(dataSize);
		auto result = Vulture::Device::vkGetRayTracingShaderGroupHandlesKHR(Vulture::Device::GetDevice(), m_Info.RayTracingPipeline->GetPipeline(), 0, handleCount, dataSize, handles.data());
		VL_CORE_ASSERT(result == VK_SUCCESS, "Failed to get shader group handles!");

		// Allocate a buffer for storing the SBT.
		VkDeviceSize sbtSize = m_Info.RgenRegion.size + m_Info.MissRegion.size + m_Info.HitRegion.size + m_Info.CallRegion.size;

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
		m_Info.RtSBTBuffer.Init(bufferInfo);

		// Find the SBT addresses of each group
		m_Info.RgenRegion.deviceAddress = m_Info.RtSBTBuffer.GetDeviceAddress();
		m_Info.MissRegion.deviceAddress = m_Info.RtSBTBuffer.GetDeviceAddress() + m_Info.RgenRegion.size;
		m_Info.HitRegion.deviceAddress = m_Info.RtSBTBuffer.GetDeviceAddress() + m_Info.RgenRegion.size + m_Info.MissRegion.size;
		m_Info.CallRegion.deviceAddress = m_Info.RtSBTBuffer.GetDeviceAddress() + m_Info.RgenRegion.size + m_Info.MissRegion.size + m_Info.HitRegion.size;

		auto getHandle = [&](uint32_t i) { return handles.data() + (i * handleSize); };

		// TODO maybe use WriteToBuffer?
		stagingBuffer.Map();
		uint8_t* pSBTBuffer = reinterpret_cast<uint8_t*>(stagingBuffer.GetMappedMemory());
		uint8_t* pData = nullptr;
		uint32_t handleIdx = 0;

		// Ray gen
		pData = pSBTBuffer;

		for (uint32_t c = 0; c < m_Info.RGenCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData = pSBTBuffer + m_Info.RgenRegion.stride;
		}

		// Miss
		for (uint32_t c = 0; c < m_Info.MissCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData += m_Info.MissRegion.stride;
		}

		// Hit
		pData = pSBTBuffer + m_Info.RgenRegion.size + m_Info.MissRegion.size;
		for (uint32_t c = 0; c < m_Info.HitCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData += m_Info.HitRegion.stride;
		}

		// Callable
		pData = pSBTBuffer + m_Info.RgenRegion.size + m_Info.MissRegion.size + m_Info.HitRegion.size;
		for (uint32_t c = 0; c < m_Info.CallableCount; c++)
		{
			memcpy(pData, getHandle(handleIdx), handleSize);
			handleIdx++;
			pData += m_Info.CallRegion.stride;
		}

		// Copy the shader binding table to the device local buffer
		Vulture::Buffer::CopyBuffer(stagingBuffer.GetBuffer(), m_Info.RtSBTBuffer.GetBuffer(), sbtSize, Vulture::Device::GetGraphicsQueue(), 0, Vulture::Device::GetGraphicsCommandPool());

		stagingBuffer.Unmap();

		m_Info.Initialized = true;
	}

	void SBT::Destroy()
	{

	}

	SBT::~SBT()
	{
		if (m_Info.Initialized)
			Destroy();
	}

	SBT::SBT(CreateInfo* createInfo)
	{
		Init(createInfo);
	}

}