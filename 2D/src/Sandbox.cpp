#include "pch.h"
#include "Sandbox.h"
#include "TestScript.h"
#include "CameraScript.h"
#include "TypingScript.h"

PathTracer::PathTracer(Vulture::ApplicationInfo appInfo, float width, float height)
	: Vulture::Application(appInfo, width, height), m_Scene(m_Window)
{
	Init();
	InitScripts();
}

PathTracer::~PathTracer()
{
	DestroyScripts();
}

void PathTracer::Destroy()
{

}

void PathTracer::OnUpdate(double deltaTime)
{
	if (Vulture::Input::IsKeyPressed(VL_KEY_ESCAPE))
	{
		m_Window->Close();
		return;
	}

	UpdateScripts(deltaTime);

	m_SceneRenderer->Render(m_Scene);
}

void PathTracer::InitScripts()
{
	m_Scene.InitScripts();
}

void PathTracer::UpdateScripts(double deltaTime)
{
	m_Scene.UpdateScripts(deltaTime);
}

void PathTracer::DestroyScripts()
{
	m_Scene.DestroyScripts();
}

void PathTracer::Init()
{
	m_Scene.CreateAtlas("assets/Texture.png");
	m_Scene.AddFont("assets/Pixel.ttf", "PixelFont");
	{
		auto& entity = m_Scene.CreateEntity();
		Vulture::Text::CreateInfo info{};
		info.Color = glm::vec4(1.0f);
		info.FontAtlas = m_Scene.GetFontAtlas("PixelFont");
		info.KerningOffset = 0;
		info.MaxLettersCount = 15;
		info.Resizable = true;
		info.Text = std::string("TEST");
		entity.AddComponent<Vulture::TextComponent>(info);
		entity.AddComponent<Vulture::TransformComponent>(Vulture::Transform({ 3.0f, -10.0f, -8.0f }, glm::vec3(0.0f), glm::vec3(2.0f)));
		auto& sc = entity.AddComponent<Vulture::ScriptComponent>();
		sc.AddScript<TypingScript>();

		TypingScript* ts = sc.GetScript<TypingScript>(0);
		ts->strings.push_back("Vulture");
		ts->strings.push_back("Engine...");
	}

	for (int i = 0; i < 10; i++)
	{
		for (int j = 0; j < 10; j++)
		{
			m_Scene.CreateStaticSprite({ { 2.0f * i, 2.0f * j, -20.0f}, glm::vec3(0.0f), glm::vec3(1.0f) }, {0, 0});
		}
	}
	auto& entity = m_Scene.CreateSprite({ { -2.0f, -10.0f, -10.0f }, glm::vec3(0.0f), glm::vec3(2.0f) }, {1, 0});
	entity.AddComponent<Vulture::ColliderComponent>(entity, "Player");
	auto& scComponent = entity.AddComponent<Vulture::ScriptComponent>();
	scComponent.AddScript<TestScript>();

	auto& collider = m_Scene.CreateSprite({ { -7.0f, -10.0f, -10.0f }, glm::vec3(0.0f), glm::vec3(1.0f, 10.0f, 0.0f) }, { 2, 0 });
	collider.AddComponent<Vulture::ColliderComponent>(collider, "Wall");

	Vulture::Entity camera = m_Scene.CreateCamera();
	auto& cameraScComponent = camera.AddComponent<Vulture::ScriptComponent>();
	cameraScComponent.AddScript<CameraScript>();

	m_SceneRenderer = std::make_unique<SceneRenderer>();
	m_SceneRenderer->UpdateStaticStorageBuffer(m_Scene);
}
