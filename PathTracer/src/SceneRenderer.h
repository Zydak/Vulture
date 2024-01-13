#pragma once
#include "pch.h"
#include <Vulture.h>

struct GlobalUbo
{
	glm::mat4 ViewProjectionMat;
	glm::mat4 ViewInverse;
	glm::mat4 ProjInverse;
};

struct PushConstantRay
{
	glm::vec4 ClearColor;
	int frame;
	int maxDepth;
};

struct PushConstantGBuffer
{
	glm::mat4 Model;
	Vulture::Material Material;
};

struct MeshAdresses
{
	uint64_t VertexAddress; // Address of the Vertex buffer
	uint64_t IndexAddress; // Address of the index buffer
};

class SceneRenderer
{
public:
	SceneRenderer();
	~SceneRenderer();

	void Render(Vulture::Scene& scene);

	void CreateRayTracingUniforms(Vulture::Scene& scene);
private:
	void RayTrace(const glm::vec4& clearColor);
	void DrawGBuffer();
	void Denoise();
	void ResetFrame();

	void RecreateResources();
	void FixCameraAspectRatio();

	void CreateRenderPasses();
	void CreateUniforms();
	void RecreateUniforms();
	void CreatePipelines();
	void CreateRayTracingPipeline();
	void CreateShaderBindingTable();
	void CreateFramebuffers();
	void UpdateUniformData();

	void ImGuiPass();

	enum GBufferImage
	{
		Albedo,
		Roughness,
		Metallness,
		Normal,
		Count
	};
	Vulture::Ref<Vulture::Framebuffer> m_GBufferFramebuffer;

	Vulture::Ref<Vulture::Image> m_DenoisedImage;
	Vulture::Ref<Vulture::Framebuffer> m_HDRFramebuffer; // we have only one framebuffer for ray tracing
	Vulture::Ref<Vulture::Uniform> m_HDRUniforms; // we have only one framebuffer for ray tracing

	Vulture::Ref<Vulture::Uniform> m_RayTracingUniforms; // we have only one uniform for ray tracing
	std::vector<Vulture::Ref<Vulture::Uniform>> m_GlobalUniforms;
	Vulture::Pipeline m_RtPipeline;
	
	// SBT
	VkImageView m_PresentedImageView;
	Vulture::Ref<Vulture::Buffer> m_RtSBTBuffer;
	VkStridedDeviceAddressRegionKHR m_RgenRegion{};
	VkStridedDeviceAddressRegionKHR m_MissRegion{};
	VkStridedDeviceAddressRegionKHR m_HitRegion{};
	VkStridedDeviceAddressRegionKHR m_CallRegion{};
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_RtShaderGroups;

	Timer m_Timer;
	Vulture::RenderPass m_HDRPass;
	Vulture::RenderPass m_GBufferPass;
	Vulture::Scene* m_CurrentSceneRendered;

	float m_Time = 0;
	Timer m_TotalTimer;

	int m_MaxRayDepth = 5;
	PushConstantRay m_PushContantRay{};

	VkFence m_DenoiseFence;
	uint64_t m_DenoiseFenceValue = 0U;
	Vulture::Ref<Vulture::Denoiser> m_Denoiser;
	uint32_t m_CurrentSamplesPerPixel = 0;
	int m_MaxSamplesPerPixel = 15'000;

	bool m_RunDenoising = false;
	bool m_ShowDenoised = false;
};