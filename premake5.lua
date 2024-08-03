include "lib/glfw"
include "lib/imgui"
include "lib/spdlog"
include "lib/msdf-atlas-gen"
include "lib/assimp"
include "lib/lodepng"
include "lib/imNodeFlow"

project "Vulture"
	architecture "x86_64"
    kind "StaticLib"
    language "C++"
	cppdialect "C++20"
	staticruntime "on"

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

        "lib/shaderc/glslc/file_includer.cc",
    }

    includedirs 
    {
        "src/",
        "src/Vulture/",
        "lib/shaderc/include/",

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
        "lodepng",
        "imNodeFlow",
    }

    defines { "_SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS" }
    
    buildoptions { "/MP" }

    filter "platforms:Windows"
        system "Windows"
        defines { "WIN", "VK_USE_PLATFORM_WIN32_KHR" }

    filter "platforms:Linux"
        system "Linux"
        defines "LIN"

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"
        links {"lib/shaderc/shadercDebug.lib"}

    filter "configurations:Release"
        defines { "NDEBUG" }
		runtime "Release"
        optimize "Full"
        links {"lib/shaderc/shadercRelease.lib"}

    filter "configurations:Distribution"
		defines "DISTRIBUTION"
		runtime "Release"
		optimize "Full"
        links {"lib/shaderc/shadercRelease.lib"}