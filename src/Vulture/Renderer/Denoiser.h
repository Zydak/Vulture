#pragma once

#include <array>

#include <cuda.h>
#include <cuda_runtime.h>

#include "imgui.h"

#include "optix_types.h"
#include <driver_types.h>

#include "Vulkan/Image.h"
#include "Vulkan/Sampler.h"

namespace Vulture
{

static void contextLogCb(unsigned int level, const char* tag, const char* message, void* /*cbdata */)
{
    VL_CORE_INFO("[{0}][{1}]:{2}", level, tag, message);
}

// TODO: rework this
class Denoiser
{
public:
	void Init();
	void Destroy();

	Denoiser() = default;
	~Denoiser();

	void DenoiseImageBuffer(uint64_t& fenceValue, float blendFactor = 0.0f);
	void CreateSemaphore();

	inline VkSemaphore GetTLSemaphore() const { return m_Semaphore.Vk; }

	void AllocateBuffers(const VkExtent2D& imgSize);
	void BufferToImage(const VkCommandBuffer& cmdBuf, Vulture::Image* imgOut);
	void ImageToBuffer(VkCommandBuffer& cmdBuf, const std::vector<Vulture::Image*>& imgIn);

private:
	void DestroyBuffer();

    Sampler m_Sampler;

    OptixDeviceContext     m_OptixDevice{};
    OptixDenoiser          m_Denoiser{};
    OptixDenoiserOptions   m_DenoiserOptions{};
    OptixDenoiserSizes     m_DenoiserSizes{};
    OptixDenoiserAlphaMode m_DenoiserAlpha{OPTIX_DENOISER_ALPHA_MODE_COPY};
    OptixPixelFormat       m_PixelFormat{};
    
    CUdeviceptr m_StateBuffer{};
    CUdeviceptr m_ScratchBuffer{};
    CUdeviceptr m_Intensity{};
    CUdeviceptr m_MinRGB{};
    CUstream    m_CudaStream{};
    
    VkExtent2D m_ImageSize{};
    uint32_t   m_SizeofPixel{};
    
	struct BufferCuda
	{
		Vulture::Buffer BufferVk;
		#ifdef WIN
		HANDLE Handle = nullptr;  // The Win32 handle
		#else
		int Handle = -1;
		#endif
		void* CudaPtr = nullptr;  // Pointer for cuda

		void Destroy();
	};

	std::array<BufferCuda, 3> m_PixelBufferIn;  // Buffers for the input images
	BufferCuda m_PixelBufferOut; // Result of the denoiser

	struct Semaphore
	{
		VkSemaphore Vk{};  // Vulkan
		cudaExternalSemaphore_t Cu{};  // Cuda version
		#ifdef WIN
		HANDLE Handle{ INVALID_HANDLE_VALUE };
		#else
		int Handle{ -1 };
		#endif
	};

	Semaphore m_Semaphore;

	void CreateBufferHandles(BufferCuda& buf);

	bool m_Initialized = false;
};

}  // namespace Vulture