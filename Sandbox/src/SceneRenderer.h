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

class SceneRenderer
{
public:
	SceneRenderer();
	~SceneRenderer();

	void Render(Vulture::Scene& scene);

private:

	void RecreateResources();
	void FixCameraAspectRatio();
	void UpdateStorageBuffer();

	void CreateRenderPasses();
	void CreateUniforms();
	void CreatePipelines();
	void CreateFramebuffers();

	void GeometryPass();

	Vulture::RenderPass m_HDRPass;
	std::vector<Vulture::Scope<Vulture::Framebuffer>> m_HDRFramebuffer;
	Vulture::Scene* m_CurrentSceneRendered;

	std::vector<StorageBufferEntry> m_StorageBuffer;
	std::vector<Vulture::Ref<Vulture::Uniform>> m_ObjectsUbos;
	std::vector<Vulture::Ref<Vulture::Uniform>> m_HDRUniforms;
	std::shared_ptr<Vulture::DescriptorSetLayout> m_AtlasSetLayout;
};