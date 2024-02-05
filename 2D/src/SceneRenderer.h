#pragma once
#include "pch.h"
#include <Vulture.h>

struct MainUbo
{
	glm::mat4 ViewProjMatrix;
};

struct StorageBufferEntry
{
	glm::mat4 ModelMatrix;
	glm::vec4 AtlasOffset; // vec2
};

struct StorageBufferCacheEntry
{
	Vulture::Transform Transform;
	entt::entity Entity;
};

struct TextPushConstant
{
	glm::vec4 Color;
};

class SceneRenderer
{
public:
	SceneRenderer();
	~SceneRenderer();

	void Render(Vulture::Scene& scene);
	void UpdateStaticStorageBuffer(Vulture::Scene& scene);

	void DestroySprite(entt::entity entity, Vulture::Scene& scene);

private:

	void RecreateResources();
	void FixCameraAspectRatio();
	void UpdateStorageBuffer();
	void UpdateTextStorageBuffers();
	void UpdateTextBuffers();

	void CreateRenderPasses();
	void CreateDescriptorSets();
	void RecreateDescriptorSets();
	void CreatePipelines();
	void CreateFramebuffers();

	void ImGuiPass();
	void GeometryPass();

	Timer m_Timer;
	Vulture::Scene* m_CurrentSceneRendered;
	uint32_t m_StaticObjectsCount = 0;

	Vulture::RenderPass m_HDRPass;
	Vulture::Pipeline m_FontPipeline;
	std::vector<Vulture::Scope<Vulture::Framebuffer>> m_HDRFramebuffer;

	std::vector<std::vector<StorageBufferCacheEntry>> m_StorageBufferTransforms;
	std::vector<std::vector<StorageBufferCacheEntry>> m_TextStorageBufferTransforms;

	std::vector<Vulture::Ref<Vulture::DescriptorSet>> m_ObjectsUbos;
	std::vector<Vulture::Ref<Vulture::DescriptorSet>> m_MainUbos;
	std::vector<Vulture::Ref<Vulture::DescriptorSet>> m_TextUbos;
	Vulture::Ref<Vulture::DescriptorSet> m_StaticObjectsUbos;

	std::vector<Vulture::Ref<Vulture::DescriptorSet>> m_HDRDescriptorSet;
	std::shared_ptr<Vulture::DescriptorSetLayout> m_AtlasSetLayout;
	std::shared_ptr<Vulture::DescriptorSetLayout> m_FontAtlasSetLayout;
};