include "lib/glfw"
include "lib/imgui"
include "lib/spdlog"
include "lib/msdf-atlas-gen"

project "Vulture"
	architecture "x86_64"
    kind "StaticLib"
    language "C++"
	cppdialect "C++20"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "pch.h"
    pchsource "src/pch.cpp"

    libdirs
    {
        "lib/",
        "lib/vulkanLib/",
    }

    files 
    {
        "src/**.cpp",
        "src/**.h",
    }

    includedirs 
    {
        "src/",
        "src/Vulture/",

        "lib/",
        "lib/glfw/include/",
        "lib/imgui/",
        "lib/stbimage/",
        "lib/glm/",
        "lib/msdf-atlas-gen/",
        "lib/msdf-atlas-gen/msdfgen/",
        "lib/entt/",
        "lib/vulkanLib/Include/",
        "lib/vulkanMemoryAllocator/",
        "lib/spdlog/include/"
    }

    links
    {
        "glfw",
        "imgui",
        "msdf-atlas-gen",
        "msdfgen",
        "msdfgenfreetype",
        "vulkan-1",
        "spdlog"
    }

    filter "platforms:Windows"
        system "Windows"
        defines { "WIN" }

    filter "platforms:Linux"
        system "Linux"
        defines "LIN"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "speed"

    filter "configurations:Dist"
        defines { "DIST" }
        optimize "speed"