#define VL_ENTRY_POINT

#include <Vulture.h>
#include "Sandbox.h"

Vulture::Application* Vulture::CreateApplication()
{
	Vulture::ApplicationInfo appInfo;
	appInfo.Name = "2D Sandbox";
	appInfo.WorkingDirectory = "";
	appInfo.RayTracingSupport = false;
	return new Sandbox(appInfo, 1600, 900);
}