#define VL_ENTRY_POINT
#include <Vulture.h>
#include "SceneRenderer.h"

class Sandbox : public Vulture::Application
{
public:
	Sandbox(Vulture::ApplicationInfo appInfo, float width, float height)
		: Vulture::Application(appInfo, width, height)
	{
		m_Scene.AddTextureToAtlas("assets/Texture.png");
		m_Scene.AddTextureToAtlas("assets/Texture1.png");
		m_Scene.PackAtlas();

		m_Scene.CreateSprite({ { 2.0f, 0.0f, -10.0f }, glm::vec3(0.0f), glm::vec3(1.0f) }, "assets/Texture.png");
		m_Scene.CreateSprite({ { -2.0f, 0.0f, -20.0f }, glm::vec3(0.0f), glm::vec3(2.0f) }, "assets/Texture1.png");

		Vulture::Entity camera = m_Scene.CreateCamera();
		m_CameraCp = &camera.GetComponent<Vulture::CameraComponent>();
		m_CameraCp->SetPerspectiveMatrix(45.0f, 1.0f, 0.1f, 100.0f);
	}

	~Sandbox()
	{

	}

	void OnUpdate(double delta) override
	{
		if (Vulture::Input::IsKeyPressed(VL_KEY_ESCAPE))
		{
			m_Window->Close();
			return;
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_A))
		{
			m_CameraCp->Translation.x += 0.05;
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_D))
		{
			m_CameraCp->Translation.x -= 0.05;
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_W))
		{
			m_CameraCp->Translation.y -= 0.05;
		}
		if (Vulture::Input::IsKeyPressed(VL_KEY_S))
		{
			m_CameraCp->Translation.y += 0.05;
		}

		m_CameraCp->UpdateViewMatrix();

		m_SceneRenderer.Render(m_Scene);
	}
private:
	Vulture::Scene m_Scene;
	Vulture::CameraComponent* m_CameraCp;

	SceneRenderer m_SceneRenderer;
};

Vulture::Application* Vulture::CreateApplication()
{
	Vulture::ApplicationInfo appInfo;
	appInfo.Name = "Vulture Sandbox";
	appInfo.WorkingDirectory = "";
	appInfo.Icon = "assets/Texture.png";
	return new Sandbox(appInfo, 600, 600);
}