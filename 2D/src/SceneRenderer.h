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

	Vulture::Timer m_Timer;
	Vulture::Scene* m_CurrentSceneRendered;
	uint32_t m_StaticObjectsCount = 0;

	Vulture::Pipeline m_FontPipeline;
	Vulture::Pipeline m_HDRPipeline;
	std::vector<Vulture::Ref<Vulture::Framebuffer>> m_HDRFramebuffer;

	std::vector<std::vector<StorageBufferCacheEntry>> m_StorageBufferTransforms;
	std::vector<std::vector<StorageBufferCacheEntry>> m_TextStorageBufferTransforms;

	std::vector<Vulture::DescriptorSet> m_ObjectsUbos;
	std::vector<Vulture::DescriptorSet> m_MainUbos;
	std::vector<Vulture::DescriptorSet> m_TextUbos;
	Vulture::DescriptorSet m_StaticObjectsUbos;

	std::vector<Vulture::DescriptorSet> m_HDRDescriptorSet;
	Vulture::DescriptorSetLayout m_AtlasSetLayout;
	Vulture::DescriptorSetLayout m_FontAtlasSetLayout;
};