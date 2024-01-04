project "2D"
	architecture "x86_64"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"
	staticruntime "off"

	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

	files
	{
		"src/**.h",
		"src/**.cpp"
	}

	includedirs
	{
		"%{wks.location}/Vulture/src/",
		"%{wks.location}/Vulture/src/Vulture",
		"%{wks.location}",
		"%{wks.location}/Vulture/lib/",
        "%{wks.location}/Vulture/lib/glfw/include/",
        "%{wks.location}/Vulture/lib/imgui/",
        "%{wks.location}/Vulture/lib/stbimage/",
        "%{wks.location}/Vulture/lib/glm/",
        "%{wks.location}/Vulture/lib/msdf-atlas-gen/",
        "%{wks.location}/Vulture/lib/msdf-atlas-gen/msdfgen/",
        "%{wks.location}/Vulture/lib/entt/",
        "%{wks.location}/Vulture/lib/vulkanLib/Include/",
        "%{wks.location}/Vulture/lib/vulkanMemoryAllocator/",
        "%{wks.location}/Vulture/lib/spdlog/include/",
        "%{wks.location}/Vulture/lib/tinyobjloader/",
        "%{wks.location}/Vulture/lib/assimp/include/",
	}	 

	links
	{
		"Vulture",
	}

	filter "system:windows"
		defines "WIN"
		systemversion "latest"

	filter "configurations:Debug"
		defines "DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "RELEASE"
		runtime "Release"
		optimize "on"

	filter "configurations:Dist"
		defines "DIST"
		runtime "Release"
		optimize "on"