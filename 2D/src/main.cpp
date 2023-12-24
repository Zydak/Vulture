#define VL_ENTRY_POINT
#include <Vulture.h>
#include "Sandbox.h"

Vulture::Application* Vulture::CreateApplication()
{
	Vulture::ApplicationInfo appInfo;
	appInfo.Name = "Vulture Sandbox";
	appInfo.WorkingDirectory = "";
	return new Sandbox(appInfo, 600, 600);
}