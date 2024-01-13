include "lib/glfw"
include "lib/imgui"
include "lib/spdlog"
include "lib/msdf-atlas-gen"
include "lib/assimp"

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

        globalIncludes,
    }

    links
    {
        "glfw",
        "imgui",
        "msdf-atlas-gen",
        "msdfgen",
        "msdfgenfreetype",
        "vulkan-1",
        "spdlog",
        "assimp",
        "lib/cuda/*.lib",
    }

    filter "platforms:Windows"
        system "Windows"
        defines { "WIN", "VK_USE_PLATFORM_WIN32_KHR" }

    filter "platforms:Linux"
        system "Linux"
        defines "LIN"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "speed"

    filter "configurations:Distribution"
		defines "DISTRIBUTION"
		runtime "Release"
		optimize "on"