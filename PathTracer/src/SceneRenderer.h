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
	int SamplesPerFrame;

	float FocalLength;
	float DoFStrength;
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

	void CreateRayTracingDescriptorSets(Vulture::Scene& scene);
	void SetSkybox(Vulture::SkyboxComponent& skybox);
private:
	bool RayTrace(const glm::vec4& clearColor);
	void DrawGBuffer();
	void Denoise();
	void ResetFrame();

	void RecreateResources();
	void FixCameraAspectRatio();

	void CreateRenderPasses();
	void CreateDescriptorSets();
	void RecreateDescriptorSets();
	void CreatePipelines();
	void CreateRayTracingPipeline();
	void CreateShaderBindingTable();
	void CreateFramebuffers();
	void UpdateDescriptorSetsData();

	void CreateHDRSet();

	void ImGuiPass();

	enum GBufferImage
	{
		Albedo,
		Normal,
		RoughnessMetallness,
		Count
	};
	Vulture::Ref<Vulture::Framebuffer> m_GBufferFramebuffer;
	bool m_DrawGBuffer = true;

	Vulture::Ref<Vulture::Image> m_Skybox;

	Vulture::Ref<Vulture::Image> m_DenoisedImage;
	Vulture::Ref<Vulture::Image> m_PathTracingImage;
	Vulture::Ref<Vulture::DescriptorSet> m_HDRDescriptorSet;
	Vulture::Ref<Vulture::DescriptorSet> m_ToneMapDescriptorSet;

	Vulture::Ref<Vulture::DescriptorSet> m_RayTracingDescriptorSet; // there is only one set for ray tracing
	std::vector<Vulture::Ref<Vulture::DescriptorSet>> m_GlobalDescriptorSets;
	Vulture::Pipeline m_RtPipeline;
	
	Vulture::Ref<Vulture::Image> m_PresentedImage;

	Vulture::SBT m_SBT;

	Timer m_Timer;
	Vulture::RenderPass m_HDRPass;
	Vulture::RenderPass m_GBufferPass;
	Vulture::Scene* m_CurrentSceneRendered;

	float m_Time = 0;
	Timer m_TotalTimer;

	float m_Exposure = 1.0f;

	int m_MaxRayDepth = 20;
	PushConstantRay m_PushContantRay{};

	VkFence m_DenoiseFence;
	uint64_t m_DenoiseFenceValue = 0U;
	Vulture::Ref<Vulture::Denoiser> m_Denoiser;
	uint32_t m_CurrentSamplesPerPixel = 0;
	int m_MaxSamplesPerPixel = 1500;

	VkDescriptorSet m_ImGuiViewportDescriptor;
	VkDescriptorSet m_ImGuiNormalDescriptor;
	VkDescriptorSet m_ImGuiAlbedoDescriptor;
	VkDescriptorSet m_ImGuiRoughnessDescriptor;
	VkDescriptorSet m_ImGuiEmissiveDescriptor;
	VkExtent2D m_ImGuiViewportSize = { 942, 881 };
	VkExtent2D m_ViewportSize = { 942, 881 };
	bool m_ImGuiViewportResized = false;
	int m_SamplesPerFrame = 15;

	bool m_RunDenoising = false;
	bool m_ShowDenoised = false;
	bool m_Denoised = false;

	float m_FocalLength = 1.0f;
	float m_DoFStrength = 0.0f;

	bool m_ToneMapped = false;
	
	bool m_DrawIntoAFile = false;
	bool m_DrawIntoAFileChanged = false;

	int m_BloomMipsCount = 6;

	struct DrawFileInfo
	{
		int Resolution[2] = { 942, 881 };
		int TotalSamplesPerPixel = 50'000;
		int SamplesPerFrame = 15;
		int RayDepth = 20;
		int MipsCount = 6;
		float Exposure = 1.0f;
		float DOFStrength = 0.0f;
		float FocalLenght = 8.0f;

		bool DrawingFramebufferFinished = false;
	};

	DrawFileInfo m_DrawFileInfo{};
};