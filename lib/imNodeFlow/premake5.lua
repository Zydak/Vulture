project "imnodeflow"
	architecture "x86_64"
    kind "StaticLib"
    language "C++"
	cppdialect "C++20"
	staticruntime "on"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

    files 
    {
        "src/**.cpp",
        "src/**.h",
    }

    includedirs
    {
        "include/",
        "../imgui"
    }

    defines
    {
        "IMGUI_DEFINE_MATH_OPERATORS"
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