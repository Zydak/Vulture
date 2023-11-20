#include <Vulture.h>

class Sandbox : public Vulture::Application
{
public:
	Sandbox(Vulture::ApplicationInfo appInfo, float width, float height)
		: Vulture::Application(appInfo, width, height)
	{
		m_Scene.AddTextureToAtlas("assets/Texture.png");
		m_Scene.AddTextureToAtlas("assets/Texture1.png");
		m_Scene.PackAtlas();

		m_Scene.CreateSprite({ { 0.5f, 0.0f, 0.0f }, glm::vec3(0.0f), glm::vec3(0.5f) }, "assets/Texture.png");
		m_Scene.CreateSprite({ { -0.5f, 0.0f, 0.0f }, glm::vec3(0.0f, 0.0f, 45.0f), glm::vec3(0.5f) }, "assets/Texture1.png");
	}

	~Sandbox()
	{

	}

	void OnUpdate(double delta) override
	{
		Vulture::Renderer::Render(m_Scene);
	}
private:
	Vulture::Scene m_Scene;
};

Vulture::Application* Vulture::CreateApplication()
{
	Vulture::ApplicationInfo appInfo;
	appInfo.Name = "Vulture Sandbox";
	appInfo.WorkingDirectory = "";
	appInfo.Icon = "assets/Texture.png";
	return new Sandbox(appInfo, 1600, 900);
}