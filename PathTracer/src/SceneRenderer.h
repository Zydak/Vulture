#pragma once
#include "pch.h"
#include <Vulture.h>

struct GlobalUbo
{
	glm::mat4 ViewProjectionMat;
	glm::mat4 ViewInverse;
	glm::mat4 ProjInverse;
};

struct StipplingPushContant
{
	int NoiseCenterPixelWeight;
	int NoiseSampleRange;
	float LuminanceBias;
};

struct PushConstantRay
{
	glm::vec4 ClearColor;
	int64_t frame;
	int maxDepth;
	int SamplesPerFrame;
	float EnvAzimuth;
	float EnvAltitude;

	float FocalLength;
	float DoFStrength;
	float AliasingJitter;
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
	void SetSkybox(Vulture::Entity& skyboxEntity);
private:
	void RecreateRayTracingDescriptorSets();
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

	void ImGuiPass();

	enum GBufferImage
	{
		Albedo,
		Normal,
		RoughnessMetallness,
		Emissive,
		Depth,
		Count
	};
	Vulture::Ref<Vulture::Framebuffer> m_GBufferFramebuffer;
	Vulture::Pipeline m_GBufferPipeline;

	Vulture::Ref<Vulture::Image> m_Skybox;

	Vulture::Ref<Vulture::Image> m_DenoisedImage;
	Vulture::Ref<Vulture::Image> m_PathTracingImage;

	Vulture::Ref<Vulture::DescriptorSet> m_RayTracingDescriptorSet; // there is only one set for ray tracing
	std::vector<Vulture::Ref<Vulture::DescriptorSet>> m_GlobalDescriptorSets;
	Vulture::Pipeline m_RtPipeline;
	
	Vulture::Ref<Vulture::Image> m_PresentedImage;
	Vulture::Ref<Vulture::Image> m_TonemappedImage;
	Vulture::Ref<Vulture::Image> m_BloomImage;
	Vulture::Ref<Vulture::Image> m_InkEffectImage;

	Vulture::Ref<Vulture::Image> m_BlueNoiseImage;
	Vulture::Ref<Vulture::Image> m_PaperTexture;
	Vulture::Ref<Vulture::Image> m_InkTexture;

	enum class CurrentPostProcess
	{
		None, // Bloom And Tonemap
		Ink, // Bloom And Tonemap With Ink Effect
	};

	CurrentPostProcess m_CurrentEffect = CurrentPostProcess::None;

	Vulture::SBT m_SBT;

	Vulture::PushConstant<PushConstantGBuffer> m_PushContantGBuffer;
	Vulture::PushConstant<PushConstantRay> m_PushContantRayTrace;

	Vulture::Scene* m_CurrentSceneRendered;

	VkFence m_DenoiseFence;
	uint64_t m_DenoiseFenceValue = 0U;
	Vulture::Ref<Vulture::Denoiser> m_Denoiser;
	Vulture::Tonemap m_Tonemapper;
	Vulture::Tonemap m_DenoisedTonemapper;
	Vulture::Bloom m_Bloom;
	Vulture::Bloom m_DenoisedBloom;

	StipplingPushContant m_InkPush = {2, 1, 0.0f};
	Vulture::Effect<StipplingPushContant> m_InkEffect;

	std::string m_CurrentHitShaderPath = "src/shaders/CookTorrance.rchit";
	bool m_RecreateRtPipeline = false;

	// ImGui Stuff / Interface
	Vulture::Entity m_CurrentSkyboxEntity;
	std::string m_SkyboxPath;
	bool m_ChangeSkybox = false;

	float m_ModelScale = 0.5f;
	std::string m_ModelPath = "";
	bool m_ModelChanged = false;
	Vulture::Entity CurrentModelEntity;
	std::vector<Vulture::Material>* m_CurrentMaterials;
	std::vector<std::string> m_CurrentMeshesNames;

	Vulture::Timer m_Timer;
	Vulture::Timer m_TotalTimer;
	uint64_t m_CurrentSamplesPerPixel = 0;
	VkDescriptorSet m_ImGuiViewportDescriptorTonemapped;
	VkDescriptorSet m_ImGuiViewportDescriptorPathTracing;
	VkDescriptorSet m_ImGuiViewportDescriptorInk;
	VkDescriptorSet m_ImGuiNormalDescriptor;
	VkDescriptorSet m_ImGuiAlbedoDescriptor;
	VkDescriptorSet m_ImGuiRoughnessDescriptor;
	VkDescriptorSet m_ImGuiEmissiveDescriptor;
	VkExtent2D m_ViewportSize = { 1920, 1080 };
	VkExtent2D m_ViewportContentSize = { 1920, 1080 };
	bool m_ImGuiViewportResized = false;
	float m_Time = 0;

	bool m_RunDenoising = false;
	bool m_ShowDenoised = false;
	bool m_Denoised		= false;

	bool m_ToneMapped      = false;
	bool m_DrawGBuffer     = true;
	bool m_HasEnvMap	   = false;
	bool m_RecompileShader = false;

	struct DrawInfo
	{
		float DOFStrength			= 0.0f;
		float FocalLength			= 8.0f;
		bool AutoDoF				= false;
		float AliasingJitterStr		= 1.0f;
		int TotalSamplesPerPixel	= 15000;
		int RayDepth				= 20;
		int SamplesPerFrame			= 15;
		bool UseNormalMaps			= false;
		bool UseAlbedo				= true;
		bool UseGlossy				= true;
		bool UseGlass				= true;
		bool UseClearcoat			= true;
		bool UseFog					= false;
		bool UseFireflies			= true;

		bool SampleEnvMap   = true;
		float EnvAzimuth	= 0.0f;
		float EnvAltitude	= 0.0f;

		Vulture::Tonemap::TonemapInfo TonemapInfo{};
		Vulture::Bloom::BloomInfo BloomInfo{};
	};

	bool m_DrawIntoAFile = false;
	bool m_DrawIntoAFileFinished = false;
	bool m_DrawIntoAFileChanged = false;

	struct DrawFileInfo
	{
		int Resolution[2] = { 1920, 1080 };

		bool RenderingFinished = false;

		bool SaveToFile = false;
		bool Denoised = false;
		bool ShowDenoised = false;
	};

	DrawInfo m_DrawInfo{};
	DrawFileInfo m_DrawFileInfo{};
};