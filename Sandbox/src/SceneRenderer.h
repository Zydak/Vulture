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

	void CreateRenderPasses();
	void CreateUniforms();
	void RecreateCreateUniforms();
	void CreatePipelines();
	void CreateFramebuffers();

	void ImGuiPass();
	void GeometryPass();

	Timer m_Timer;
	Vulture::RenderPass m_HDRPass;
	std::vector<Vulture::Scope<Vulture::Framebuffer>> m_HDRFramebuffer;
	Vulture::Scene* m_CurrentSceneRendered;

	std::vector<std::vector<StorageBufferCacheEntry>> m_StorageBufferTransforms;
	std::vector<Vulture::Ref<Vulture::Uniform>> m_ObjectsUbos;
	std::vector<Vulture::Ref<Vulture::Uniform>> m_MainUbos;
	Vulture::Ref<Vulture::Uniform> m_StaticObjectsUbos;
	uint32_t m_StaticObjectsCount = 0;
	std::vector<Vulture::Ref<Vulture::Uniform>> m_HDRUniforms;
	std::shared_ptr<Vulture::DescriptorSetLayout> m_AtlasSetLayout;
};