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

	m_SceneRenderer.Render(m_Scene);
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
	
	auto& entity = m_Scene.CreateEntity();
	auto& mesh = entity.AddComponent<Vulture::MeshComponent>();
	auto& transform = entity.AddComponent<Vulture::TransformComponent>(glm::vec3(0.0f, 0.0f, -10.0f), glm::vec3(45.0f, 30.0f, 0.0f), glm::vec3(1.0f));
	mesh.Mesh.CreateMesh("assets/cube.obj");
	//mesh.Mesh.CreateMesh(vertices, indices);

	m_Scene.InitAccelerationStructure();
	m_SceneRenderer.CreateRayTracingUniforms(m_Scene);
}
