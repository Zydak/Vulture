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
	float EnvAzimuth;
	float EnvAltitude;

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

	Vulture::Ref<Vulture::Image> m_Skybox;

	Vulture::Ref<Vulture::Image> m_DenoisedImage;
	Vulture::Ref<Vulture::Image> m_PathTracingImage;
	Vulture::Ref<Vulture::DescriptorSet> m_ToneMappedImageSet;
	Vulture::Ref<Vulture::DescriptorSet> m_DenoisedImageSet;

	Vulture::Ref<Vulture::DescriptorSet> m_RayTracingDescriptorSet; // there is only one set for ray tracing
	std::vector<Vulture::Ref<Vulture::DescriptorSet>> m_GlobalDescriptorSets;
	Vulture::Pipeline m_RtPipeline;
	
	bool m_ShowTonemapped = true;
	Vulture::Ref<Vulture::Image> m_PresentedImage;
	Vulture::Ref<Vulture::Image> m_TonemappedImage;
	Vulture::Ref<Vulture::Image> m_BloomImage;

	Vulture::SBT m_SBT;

	Vulture::PushConstant<PushConstantGBuffer> m_PushContantGBuffer;
	Vulture::PushConstant<PushConstantRay> m_PushContantRayTrace;

	Vulture::RenderPass m_HDRPass;
	Vulture::RenderPass m_GBufferPass;
	Vulture::Scene* m_CurrentSceneRendered;

	VkFence m_DenoiseFence;
	uint64_t m_DenoiseFenceValue = 0U;
	Vulture::Ref<Vulture::Denoiser> m_Denoiser;
	Vulture::Tonemap m_Tonemapper;
	Vulture::Bloom m_Bloom;

	// ImGui Stuff / Interface
	Timer m_Timer;
	Timer m_TotalTimer;
	uint32_t m_CurrentSamplesPerPixel = 0;
	VkDescriptorSet m_ImGuiViewportDescriptorTonemapped;
	VkDescriptorSet m_ImGuiViewportDescriptorDenoised;
	VkDescriptorSet m_ImGuiViewportDescriptorPathTracing;
	VkDescriptorSet m_ImGuiNormalDescriptor;
	VkDescriptorSet m_ImGuiAlbedoDescriptor;
	VkDescriptorSet m_ImGuiRoughnessDescriptor;
	VkDescriptorSet m_ImGuiEmissiveDescriptor;
	VkExtent2D m_ImGuiViewportSize = { 1920, 1080 };
	VkExtent2D m_ViewportSize = { 1920, 1080 };
	bool m_ImGuiViewportResized = false;
	float m_Time = 0;

	bool m_RunDenoising = false;
	bool m_ShowDenoised = false;
	bool m_Denoised = false;

	bool m_ToneMapped = false;
	bool m_DrawGBuffer = true;

	struct DrawInfo
	{
		float DOFStrength		 = 0.0f;
		float FocalLength		 = 8.0f;
		int TotalSamplesPerPixel = 15000;
		int RayDepth = 20;
		int SamplesPerFrame = 15;
		float EnvAzimuth = 0.0f;
		float EnvAltitude = 0.0f;

		Vulture::Tonemap::TonemapInfo TonemapInfo{};
		Vulture::Bloom::BloomInfo BloomInfo{};
	};

	bool m_DrawIntoAFile = false;
	bool m_DrawIntoAFileFinished = false;
	bool m_DrawIntoAFileChanged = false;

	struct DrawFileInfo
	{
		int Resolution[2] = { 1920, 1080 };

		SceneRenderer::DrawInfo DrawInfo{};

		bool DrawingFramebufferFinished = false;
	};

	DrawInfo m_DrawInfo{};
	DrawFileInfo m_DrawFileInfo{};
};