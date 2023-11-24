#include "pch.h"
#include "Sandbox.h"
#include "TestScript.h"

Sandbox::Sandbox(Vulture::ApplicationInfo appInfo, float width, float height)
	: Vulture::Application(appInfo, width, height)
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
	if (Vulture::Input::IsKeyPressed(VL_KEY_ESCAPE))
	{
		m_Window->Close();
		return;
	}

	if (Vulture::Input::IsKeyPressed(VL_KEY_A))
	{
		m_CameraCp->Translation.x += 0.05f;
	}
	if (Vulture::Input::IsKeyPressed(VL_KEY_D))
	{
		m_CameraCp->Translation.x -= 0.05f;
	}
	if (Vulture::Input::IsKeyPressed(VL_KEY_W))
	{
		m_CameraCp->Translation.y -= 0.05f;
	}
	if (Vulture::Input::IsKeyPressed(VL_KEY_S))
	{
		m_CameraCp->Translation.y += 0.05f;
	}

	m_CameraCp->UpdateViewMatrix();

	static entt::entity entity;
	static bool test = true;
	if (test)
	{
		entity = m_Scene.CreateSprite({ { 0.0f, 0.0f, -20.0f}, glm::vec3(0.0f), glm::vec3(1.0f) }, "assets/Texture1.png").GetHandle();
		test = false;
	}
	else
	{
		m_SceneRenderer.DestroySprite(entity, m_Scene);
		test = true;
	}

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
	m_Scene.AddTextureToAtlas("assets/Texture.png");
	m_Scene.AddTextureToAtlas("assets/Texture1.png");
	m_Scene.PackAtlas();

	for (int i = 0; i < 32000; i++)
	{
		m_Scene.CreateStaticSprite({ { -2.0f, 0.0f, -20.0f}, glm::vec3(0.0f), glm::vec3(1.0f)}, "assets/Texture1.png");
	}
	auto& entity = m_Scene.CreateSprite({ { 2.0f, 0.0f, -10.0f }, glm::vec3(0.0f), glm::vec3(1.0f) }, "assets/Texture.png");
	auto& scComponent = entity.AddComponent<Vulture::ScriptComponent>();
	scComponent.AddScript<TestScript>();

	Vulture::Entity camera = m_Scene.CreateCamera();
	m_CameraCp = &camera.GetComponent<Vulture::CameraComponent>();
	m_CameraCp->SetPerspectiveMatrix(45.0f, 1.0f, 0.1f, 100.0f);

	m_SceneRenderer.UpdateStaticStorageBuffer(m_Scene);
}
