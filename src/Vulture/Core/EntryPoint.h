// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

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

    return 0;
}