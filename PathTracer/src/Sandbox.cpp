#include "pch.h"
#include "Sandbox.h"
#include "CameraScript.h"

Sandbox::Sandbox(Vulture::ApplicationInfo appInfo, float width, float height)
	: Vulture::Application(appInfo, width, height), m_Scene(m_Window)
{
	Init();
	InitScripts();
}

Sandbox::~Sandbox()
{
	DestroyScripts();
}

void Sandbox::OnUpdate(double deltaTime)
{
	UpdateScripts(deltaTime);

	m_SceneRenderer->Render(m_Scene);
}

void Sandbox::InitScripts()
{
	m_Scene.InitScripts();
}

void Sandbox::UpdateScripts(double deltaTime)
{
	m_Scene.UpdateScripts(deltaTime);
}

void Sandbox::DestroyScripts()
{
	m_Scene.DestroyScripts();
}

void Sandbox::Init()
{
	// Add camera
	Vulture::Entity camera = m_Scene.CreateCamera();
	auto& cameraScComponent = camera.AddComponent<Vulture::ScriptComponent>();
	cameraScComponent.AddScript<CameraScript>();

	// Add skybox
	Vulture::Entity skybox = m_Scene.CreateEntity();
	//auto& skyboxComponent = skybox.AddComponent<Vulture::SkyboxComponent>("assets/sky.hdr");
	auto& skyboxComponent = skybox.AddComponent<Vulture::SkyboxComponent>("assets/sunrise.hdr");

	// Load Scene
	m_Scene.CreateModel("assets/cornellBox.gltf", Vulture::Transform(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.5f)));
	//m_Scene.CreateModel("assets/ship.gltf", Vulture::Transform(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.5f)));
	//m_Scene.CreateModel("assets/Sponza.gltf", Vulture::Transform(glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.5f)));

	// Initialize path tracer
	m_SceneRenderer = std::make_unique<SceneRenderer>(m_Scene);
	m_Scene.InitAccelerationStructure();
	m_SceneRenderer->CreateRayTracingUniforms(m_Scene);
}
