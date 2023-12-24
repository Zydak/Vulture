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
	glm::vec4 LightPosition;
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

	std::vector<Vulture::Ref<Vulture::Framebuffer>> m_HDRFramebuffer;
	std::vector<Vulture::Ref<Vulture::Uniform>> m_HDRUniforms;

	std::vector<Vulture::Ref<Vulture::Uniform>> m_RayTracingUniforms;
	std::vector<Vulture::Ref<Vulture::Uniform>> m_GlobalUniforms;
	Vulture::Pipeline m_RtPipeline;
	
	// SBT
	Vulture::Ref<Vulture::Buffer> m_RtSBTBuffer;
	VkStridedDeviceAddressRegionKHR m_RgenRegion{};
	VkStridedDeviceAddressRegionKHR m_MissRegion{};
	VkStridedDeviceAddressRegionKHR m_HitRegion{};
	VkStridedDeviceAddressRegionKHR m_CallRegion{};
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_RtShaderGroups;

	Timer m_Timer;
	Vulture::RenderPass m_HDRPass;
	Vulture::Scene* m_CurrentSceneRendered;
};