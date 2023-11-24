#pragma once

#include "Application.h"
#include "Vulture/src/Vulkan/Device.h"
#include "Vulture/src/Vulkan/Window.h"

extern Vulture::Application* Vulture::CreateApplication();

int main()
{
 	auto app = Vulture::CreateApplication();
 	app->Run();

    delete app;
}