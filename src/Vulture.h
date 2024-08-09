// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#pragma once

#ifdef VL_ENTRY_POINT
#include "Vulture/src/Vulture/core/EntryPoint.h"
#endif

#include "Vulture/src/Vulture/core/Application.h"
#include "Vulture/src/Vulture/core/Input.h"
#include "Vulture/src/Vulture/Scene/Components.h"
#include "Vulture/src/Vulture/Scene/Entity.h"
#include "Vulture/src/Vulture/Scene/Scene.h"
#include "Vulture/src/Vulture/Renderer/Renderer.h"
#include "Vulture/src/Vulture/Renderer/FontAtlas.h"
#include "Vulture/src/Vulture/Renderer/Text.h"
#include "Vulture/src/Vulture/Math/Transform.h"
#include "Vulture/src/Vulture/Renderer/AccelerationStructure.h"
#include "Vulture/src/Vulture/Renderer/Denoiser.h"
#include "Vulture/src/Vulkan/SBT.h"
#include "Vulture/src/Vulkan/PushConstant.h"
#include "Vulture/src/Vulkan/Shader.h"
#include "Vulture/src/Vulture/Math/Quaternion.h"

#include "Vulture/src/Vulture/Effects/Tonemap.h"
#include "Vulture/src/Vulture/Effects/Bloom.h"
#include "Vulture/src/Vulture/Effects/Effect.h"
#include "Vulture/src/Vulture/Math/Math.h"

#include "Vulture/src/Vulture/Asset/AssetManager.h"

#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
