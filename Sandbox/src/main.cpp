#include <Vulture.h>

class Sandbox : public Vulture::Application
{
public:
	Sandbox(Vulture::ApplicationInfo appInfo, float width, float height)
		: Vulture::Application(appInfo, width, height)
	{

	}

	~Sandbox()
	{

	}

	void OnUpdate(double delta) override
	{

	}
};

Vulture::Application* Vulture::CreateApplication()
{
	Vulture::ApplicationInfo appInfo;
	appInfo.Name = "Vulture Sandbox";
	appInfo.WorkingDirectory = "";
	return new Sandbox(appInfo, 600, 600);
}