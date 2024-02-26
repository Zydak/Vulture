#include "pch.h"

#include <sstream>

#include "vulkan/vulkan.h"

#include "optix.h"
#include "optix_function_table_definition.h"
#include "optix_stubs.h"

#include "Denoiser.h"

namespace Vulture
{
    Denoiser::~Denoiser()
    {
        if (m_Initialized)
            Destroy();
    }

	void Denoiser::Init()
	{
		if (m_Initialized)
			Destroy();

        m_Sampler.Init(SamplerInfo());

		VkFenceCreateInfo createInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

		CreateSemaphore();

		// Initialize CUDA
		VL_CORE_RETURN_ASSERT(cudaFree(nullptr), 0, "Couldn't reset cuda");

		CUcontext cuCtx = nullptr;  // zero means take the current context
		VL_CORE_RETURN_ASSERT(optixInit(), 0, "Couldn't initialize optix!");

		OptixDeviceContextOptions optixoptions = {};
		optixoptions.logCallbackFunction = &contextLogCb;
		optixoptions.logCallbackLevel = 4;

		VL_CORE_RETURN_ASSERT(optixDeviceContextCreate(cuCtx, &optixoptions, &m_OptixDevice), 0, "Couldn't Create optix device context");
		VL_CORE_RETURN_ASSERT(optixDeviceContextSetLogCallback(m_OptixDevice, contextLogCb, nullptr, 4), 0, "Couldn't Set Log Callback");

		// TODO: Maybe blit lower formats to higher formats before copying to buffer? Writing to buffer would be faster
		m_PixelFormat = OPTIX_PIXEL_FORMAT_FLOAT4;
		switch (m_PixelFormat)
		{
		case OPTIX_PIXEL_FORMAT_FLOAT3:
			m_SizeofPixel = (uint32_t)3 * sizeof(float);
			m_DenoiserAlpha = OPTIX_DENOISER_ALPHA_MODE_COPY;
			break;
		case OPTIX_PIXEL_FORMAT_FLOAT4:
			m_SizeofPixel = (uint32_t)4 * sizeof(float);
			m_DenoiserAlpha = OPTIX_DENOISER_ALPHA_MODE_DENOISE;
			break;
		case OPTIX_PIXEL_FORMAT_UCHAR3:
			m_SizeofPixel = (uint32_t)3 * sizeof(uint8_t);
			m_DenoiserAlpha = OPTIX_DENOISER_ALPHA_MODE_COPY;
			break;
		case OPTIX_PIXEL_FORMAT_UCHAR4:
			m_SizeofPixel = (uint32_t)4 * sizeof(uint8_t);
			m_DenoiserAlpha = OPTIX_DENOISER_ALPHA_MODE_DENOISE;
			break;
		case OPTIX_PIXEL_FORMAT_HALF3:
			m_SizeofPixel = (uint32_t)3 * sizeof(uint16_t);
			m_DenoiserAlpha = OPTIX_DENOISER_ALPHA_MODE_COPY;
			break;
		case OPTIX_PIXEL_FORMAT_HALF4:
			m_SizeofPixel = (uint32_t)4 * sizeof(uint16_t);
			m_DenoiserAlpha = OPTIX_DENOISER_ALPHA_MODE_DENOISE;
			break;
		default:
			VL_CORE_ASSERT(false, "Format Unsupported");
			break;
		}

		OptixDenoiserOptions d_options;
		d_options.guideAlbedo = 1;
		d_options.guideNormal = 1;
        d_options.denoiseAlpha = m_DenoiserAlpha;

		m_DenoiserOptions = d_options;
		OptixDenoiserModelKind modelKind = OPTIX_DENOISER_MODEL_KIND_AOV;
		VL_CORE_RETURN_ASSERT(optixDenoiserCreate(m_OptixDevice, modelKind, &m_DenoiserOptions, &m_Denoiser), 0, "Couldn't Create Optix Denoiser");
	
        m_Initialized = true;
    }

	void Denoiser::Destroy()
	{
		// Cleanup resources
		optixDenoiserDestroy(m_Denoiser);
		optixDeviceContextDestroy(m_OptixDevice);

		vkDestroySemaphore(Device::GetDevice(), m_Semaphore.Vk, nullptr);
		m_Semaphore.Vk = VK_NULL_HANDLE;

		DestroyBuffer();
        m_Initialized = false;
	}

	/**
    * @brief Performs image denoising using the OptiX denoiser on a Vulkan image buffer.
    * 
    * @param fenceValue - Reference to the fence value for synchronization.
    * @param blendFactor(Optional) - Blend factor for combining the denoised result with the original image.
    *                   A value of 0.0f results in full denoising, while 1.0f retains the original image.
    */
    void Denoiser::DenoiseImageBuffer(uint64_t& fenceValue, float blendFactor /*= 0.0f*/)
    {
        OptixPixelFormat pixelFormat = m_PixelFormat;
        auto             sizeofPixel = m_SizeofPixel;
        uint32_t         rowStrideInBytes = sizeofPixel * m_ImageSize.width;

        // Create and set OptiX layers
        OptixDenoiserLayer layer = {};
        // Input
        layer.input.data = (CUdeviceptr)m_PixelBufferIn[0].CudaPtr;
        layer.input.width = m_ImageSize.width;
        layer.input.height = m_ImageSize.height;
        layer.input.rowStrideInBytes = rowStrideInBytes;
        layer.input.pixelStrideInBytes = m_SizeofPixel;
        layer.input.format = pixelFormat;

        // Output
        layer.output.data = (CUdeviceptr)m_PixelBufferOut.CudaPtr;
        layer.output.width = m_ImageSize.width;
        layer.output.height = m_ImageSize.height;
        layer.output.rowStrideInBytes = rowStrideInBytes;
        layer.output.pixelStrideInBytes = sizeof(float) * 4;
        layer.output.format = pixelFormat;


        OptixDenoiserGuideLayer guideLayer = {};
        // albedo
        if (m_DenoiserOptions.guideAlbedo != 0u)
        {
            guideLayer.albedo.data = (CUdeviceptr)m_PixelBufferIn[1].CudaPtr;
            guideLayer.albedo.width = m_ImageSize.width;
            guideLayer.albedo.height = m_ImageSize.height;
            guideLayer.albedo.rowStrideInBytes = rowStrideInBytes;
            guideLayer.albedo.pixelStrideInBytes = m_SizeofPixel;
            guideLayer.albedo.format = pixelFormat;
        }

        // normal
        if (m_DenoiserOptions.guideNormal != 0u)
        {
            guideLayer.normal.data = (CUdeviceptr)m_PixelBufferIn[2].CudaPtr;
            guideLayer.normal.width = m_ImageSize.width;
            guideLayer.normal.height = m_ImageSize.height;
            guideLayer.normal.rowStrideInBytes = rowStrideInBytes;
            guideLayer.normal.pixelStrideInBytes = m_SizeofPixel;
            guideLayer.normal.format = pixelFormat;
        }

        // Wait from Vulkan (Copy to Buffer)
        cudaExternalSemaphoreWaitParams waitParams{};
        waitParams.flags = 0;
        waitParams.params.fence.value = fenceValue;
        VL_CORE_RETURN_ASSERT(cudaWaitExternalSemaphoresAsync(&m_Semaphore.Cu, &waitParams, 1, nullptr), 0, "Waiting for semaphore failed");

        if (m_Intensity != 0)
        {
            VL_CORE_RETURN_ASSERT(optixDenoiserComputeIntensity(m_Denoiser, m_CudaStream, &layer.input, m_Intensity, m_ScratchBuffer,
                m_DenoiserSizes.withoutOverlapScratchSizeInBytes), 0, "Computing Intensity Failed");
        }

		OptixDenoiserParams denoiserParams{};
        denoiserParams.hdrIntensity = m_Intensity;
        denoiserParams.blendFactor = blendFactor;


        // Execute the denoiser
        VL_CORE_RETURN_ASSERT(optixDenoiserInvoke(m_Denoiser, m_CudaStream, &denoiserParams, m_StateBuffer,
            m_DenoiserSizes.stateSizeInBytes, &guideLayer, &layer, 1, 0, 0, m_ScratchBuffer,
            m_DenoiserSizes.withoutOverlapScratchSizeInBytes), 0, "Denoising Failed");

        cudaDeviceScheduleBlockingSync;
        cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync);
        VL_CORE_RETURN_ASSERT(cudaDeviceSynchronize(), 0, "Synchronizing Cuda Failed");  // Making sure the denoiser is done
        VL_CORE_RETURN_ASSERT(cudaStreamSynchronize(m_CudaStream), 0, "Synchronizing Stream Failed");

        cudaExternalSemaphoreSignalParams sigParams{};
        sigParams.flags = 0;
        ++fenceValue;
        sigParams.params.fence.value = fenceValue;
        VL_CORE_RETURN_ASSERT(cudaSignalExternalSemaphoresAsync(&m_Semaphore.Cu, &sigParams, 1, m_CudaStream), 0, "Signaling Semaphore Failed");
    }

    /**
    * @brief Copies images to a buffers used by CUDA.
    *
    * @param cmdBuf The Vulkan command buffer for recording commands.
    * @param imgIn - Vector containing three images to be copied to buffers.
    *              - [0] Path tracing result.
    *              - [1] Albedo.
    *              - [2] Normals.
    */
    void Denoiser::ImageToBuffer(VkCommandBuffer& cmdBuf, const std::vector<Vulture::Image*>& imgIn)
    {
        VkBufferImageCopy region = {
            .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
            .imageExtent = {.width = m_ImageSize.width, .height = m_ImageSize.height, .depth = 1},
        };

        for (int i = 0; i < imgIn.size(); i++)
		{
            VkImageLayout prevLayout = imgIn[i]->GetLayout();

			imgIn[i]->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmdBuf);
            
            vkCmdCopyImageToBuffer(cmdBuf, imgIn[i]->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_PixelBufferIn[i].BufferVk.GetBuffer(), 1, &region);
            
            imgIn[i]->TransitionImageLayout(prevLayout, cmdBuf);
        }
    }


    /**
    * @brief Copies data from a denoised buffer to an output image.
    *
    * @param cmdBuf - Vulkan command buffer for recording commands.
    * @param imgOut - Output image where data will be copied to.
    */
    void Denoiser::BufferToImage(const VkCommandBuffer& cmdBuf, Vulture::Image* imgOut)
    {
        VkBufferImageCopy region = {
            .imageSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .layerCount = 1},
            .imageExtent = {.width = m_ImageSize.width, .height = m_ImageSize.height, .depth = 1},
        };

        imgOut->TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,cmdBuf);
        
        vkCmdCopyBufferToImage(cmdBuf, m_PixelBufferOut.BufferVk.GetBuffer(), imgOut->GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
        imgOut->TransitionImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,cmdBuf);
    }

    void Denoiser::DestroyBuffer()
    {
        if (m_StateBuffer != 0)
        {
            VL_CORE_RETURN_ASSERT(cudaFree((void*)m_StateBuffer), 0, "Couldn't Free State Buffer");
            m_StateBuffer = 0;
        }
        if (m_ScratchBuffer != 0)
        {
            VL_CORE_RETURN_ASSERT(cudaFree((void*)m_ScratchBuffer), 0, "Couldn't Free Scratch Buffer");
            m_ScratchBuffer = 0;
        }
        if (m_Intensity != 0)
        {
            VL_CORE_RETURN_ASSERT(cudaFree((void*)m_Intensity), 0, "Couldn't Free Intensity Buffer");
            m_Intensity = 0;
        }
        if (m_MinRGB != 0)
        {
            VL_CORE_RETURN_ASSERT(cudaFree((void*)m_MinRGB), 0, "Couldn't Free MinRgb Buffer");
            m_MinRGB = 0;
        }
    }

    /**
     * @brief Allocates buffers for input images (color, albedo, normal) and an output buffer.
     * It also computes the memory requirements for the denoiser and allocates necessary resources
     * such as state buffer, scratch buffer, min RGB buffer, and intensity buffer.
     *
     * @param imgSize - Size of the input images.
     * 
     * @note This function must be called before calling DenoiseImageBuffer().
     * @note This function should be called after each screen resize.
     */
    void Denoiser::AllocateBuffers(const VkExtent2D& imgSize)
    {
        m_ImageSize = imgSize;

		// Destroy existing buffers
        DestroyBuffer(); 

        VkDeviceSize outputBufferSize = ((VkDeviceSize)m_ImageSize.width) * m_ImageSize.height * 4 * sizeof(float);
        VkDeviceSize inputBufferSize = ((VkDeviceSize)m_ImageSize.width) * m_ImageSize.height * m_SizeofPixel;
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		Buffer::CreateInfo BufferInfo{};
		BufferInfo.InstanceSize = outputBufferSize;
		BufferInfo.InstanceCount = 1;
		BufferInfo.UsageFlags = usage;
		BufferInfo.MemoryPropertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        BufferInfo.NoPool = true; // Important

        // Path Tracing Result
        {
            m_PixelBufferIn[0].BufferVk.Init(BufferInfo);
            CreateBufferHandles(m_PixelBufferIn[0]);  // Exporting the buffer to Cuda handle and pointers
        }

        // Albedo
        {
            m_PixelBufferIn[1].BufferVk.Init(BufferInfo);
            CreateBufferHandles(m_PixelBufferIn[1]);
        }
        // Normal
        {
            m_PixelBufferIn[2].BufferVk.Init(BufferInfo);
            CreateBufferHandles(m_PixelBufferIn[2]);
        }

        // Output image/buffer
        BufferInfo.InstanceSize = outputBufferSize;
        m_PixelBufferOut.BufferVk.Init(BufferInfo);
        CreateBufferHandles(m_PixelBufferOut);

        // Computing the amount of memory needed to do the denoiser
        VL_CORE_RETURN_ASSERT(optixDenoiserComputeMemoryResources(m_Denoiser, m_ImageSize.width, m_ImageSize.height, &m_DenoiserSizes), 0, "Computing Sizes Failed");

        VL_CORE_RETURN_ASSERT(cudaMalloc((void**)&m_StateBuffer, m_DenoiserSizes.stateSizeInBytes), 0, "Allocating State Buffer Failed");
        VL_CORE_RETURN_ASSERT(cudaMalloc((void**)&m_ScratchBuffer, m_DenoiserSizes.withoutOverlapScratchSizeInBytes), 0, "Allocating Scratch Buffer Failed");
        VL_CORE_RETURN_ASSERT(cudaMalloc((void**)&m_MinRGB, 4 * sizeof(float)), 0, "Allocating MinRgb Buffer Failed");
        if (m_PixelFormat == OPTIX_PIXEL_FORMAT_FLOAT3 || m_PixelFormat == OPTIX_PIXEL_FORMAT_FLOAT4)
            VL_CORE_RETURN_ASSERT(cudaMalloc((void**)&m_Intensity, sizeof(float)), 0, "Allocating Intensity Buffer Failed");

        VL_CORE_RETURN_ASSERT(optixDenoiserSetup(m_Denoiser, m_CudaStream, m_ImageSize.width, m_ImageSize.height, m_StateBuffer,
            m_DenoiserSizes.stateSizeInBytes, m_ScratchBuffer, m_DenoiserSizes.withoutOverlapScratchSizeInBytes), 0, "Optix Denoiser Failed");
    }


    /**
    * @brief Creates CUDA and Win32 handles for a Vulkan buffer.
    *
    * @param buf - Valid BufferCuda object.
    */
    void Denoiser::CreateBufferHandles(BufferCuda& buf)
    {
        VkMemoryGetWin32HandleInfoKHR info{ VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
        VmaAllocationInfo memInfo = buf.BufferVk.GetMemoryInfo();
        info.memory = memInfo.deviceMemory;
        info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        VkResult res = Device::vkGetMemoryWin32HandleKHR(Device::GetDevice(), &info, &buf.Handle);

        VkMemoryRequirements memoryReq{};
        vkGetBufferMemoryRequirements(Device::GetDevice(), buf.BufferVk.GetBuffer(), &memoryReq);

        cudaExternalMemoryHandleDesc cudaExtMemHandleDesc{};
        cudaExtMemHandleDesc.size = memoryReq.size;

        cudaExtMemHandleDesc.type = cudaExternalMemoryHandleTypeOpaqueWin32;
        cudaExtMemHandleDesc.handle.win32.handle = buf.Handle;

        cudaExternalMemory_t cudaExtMemVertexBuffer{};
        VL_CORE_RETURN_ASSERT(cudaImportExternalMemory(&cudaExtMemVertexBuffer, &cudaExtMemHandleDesc), 0, "Importing External Memory Failed");

        cudaExternalMemoryBufferDesc cudaExtBufferDesc{};
        cudaExtBufferDesc.offset = 0;
        cudaExtBufferDesc.size = memoryReq.size;
        cudaExtBufferDesc.flags = 0;
        VL_CORE_RETURN_ASSERT(cudaExternalMemoryGetMappedBuffer(&buf.CudaPtr, cudaExtMemVertexBuffer, &cudaExtBufferDesc), 0, "Cuda Getting Mapped Memory Failed");
    }

    /**
    * @brief Creates a Vulkan timeline semaphore, exports its handle for CUDA, and imports it as
    * an external semaphore for CUDA interoperability. The type of handle created depends on the platform
    * (Windows or non-Windows).
    */
    void Denoiser::CreateSemaphore()
    {
#ifdef WIN
        auto handle_type = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
        auto handle_type = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

        VkSemaphoreTypeCreateInfo timelineCreateInfo{};
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = 0;

        VkExportSemaphoreCreateInfo esci{};
        esci.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO_KHR;
        esci.pNext = &timelineCreateInfo;
        esci.handleTypes = VkExternalSemaphoreHandleTypeFlags(handle_type);

        VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        sci.pNext = &esci;

        vkCreateSemaphore(Device::GetDevice(), &sci, nullptr, &m_Semaphore.Vk);

#ifdef WIN
        VkSemaphoreGetWin32HandleInfoKHR handleInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR };
        handleInfo.handleType = handle_type;
        handleInfo.semaphore = m_Semaphore.Vk;
        Device::vkGetSemaphoreWin32HandleKHR(Device::GetDevice(), &handleInfo, &m_Semaphore.Handle);
#else
        VkSemaphoreGetFdInfoKHR handleInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR };
        handleInfo.handleType = handle_type;
        handleInfo.semaphore = m_Semaphore.Vk;
        vkGetSemaphoreFdKHR(Device::GetDevice(), &handleInfo, &m_Semaphore.handle);
#endif


        cudaExternalSemaphoreHandleDesc externalSemaphoreHandleDesc{};
        std::memset(&externalSemaphoreHandleDesc, 0, sizeof(externalSemaphoreHandleDesc));
        externalSemaphoreHandleDesc.flags = 0;
#ifdef WIN
        externalSemaphoreHandleDesc.type = cudaExternalSemaphoreHandleTypeTimelineSemaphoreWin32;
        externalSemaphoreHandleDesc.handle.win32.handle = (void*)m_Semaphore.Handle;
#else
        externalSemaphoreHandleDesc.type = cudaExternalSemaphoreHandleTypeTimelineSemaphoreFd;
        externalSemaphoreHandleDesc.handle.fd = m_Semaphore.Handle;
#endif

        VL_CORE_RETURN_ASSERT(cudaImportExternalSemaphore(&m_Semaphore.Cu, &externalSemaphoreHandleDesc), 0, "Importing External Semaphore Failed");
    }

}  // namespace Vulture