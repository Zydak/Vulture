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
		m_CameraCp->Translation.x += 0.5f;
	}
	if (Vulture::Input::IsKeyPressed(VL_KEY_D))
	{
		m_CameraCp->Translation.x -= 0.5f;
	}
	if (Vulture::Input::IsKeyPressed(VL_KEY_W))
	{
		m_CameraCp->Translation.y -= 0.5f;
	}
	if (Vulture::Input::IsKeyPressed(VL_KEY_S))
	{
		m_CameraCp->Translation.y += 0.5f;
	}

	m_CameraCp->UpdateViewMatrix();

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
	m_Scene.CreateAtlas("assets/Texture.png");

	for (int i = 0; i < 10; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			m_Scene.CreateStaticSprite({ { 2.0f * i, 2.0f * j, -20.0f}, glm::vec3(0.0f), glm::vec3(1.0f) }, {0, 0});
		}
	}
	auto& entity = m_Scene.CreateSprite({ { -2.0f, -10.0f, -10.0f }, glm::vec3(0.0f), glm::vec3(2.0f) }, {1, 0});
	entity.AddComponent<Vulture::ColliderComponent>(entity);
	auto& scComponent = entity.AddComponent<Vulture::ScriptComponent>();
	scComponent.AddScript<TestScript>();

	auto& collider = m_Scene.CreateSprite({ { -7.0f, -10.0f, -10.0f }, glm::vec3(0.0f), glm::vec3(1.0f, 10.0f, 0.0f) }, { 2, 0 });
	collider.AddComponent<Vulture::ColliderComponent>(collider);

	Vulture::Entity camera = m_Scene.CreateCamera();
	m_CameraCp = &camera.GetComponent<Vulture::CameraComponent>();
	//m_CameraCp->SetPerspectiveMatrix(45.0f, 1.0f, 0.1f, 100.0f);
	m_CameraCp->SetOrthographicMatrix({ -20.0f, 20.0f, -20.0f, 20.0f }, 0.1f, 100.0f, m_Window->GetAspectRatio());

	m_SceneRenderer.UpdateStaticStorageBuffer(m_Scene);
}
