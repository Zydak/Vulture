#define VL_ENTRY_POINT
#include <Vulture.h>
#include "Sandbox.h"

Vulture::Application* Vulture::CreateApplication()
{
	Vulture::ApplicationInfo appInfo;
	appInfo.Name = "PathTracer Sandbox";
	appInfo.WorkingDirectory = "";
	appInfo.RayTracingSupport = true;
	return new Sandbox(appInfo, 1600, 900);
}