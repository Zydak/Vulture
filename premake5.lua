workspace "Vulture"
    configurations { "Debug", "Release", "Distribution" }
    platforms { "Windows", "Linux" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
	
include "Vulture"
include "2D"
include "PathTracer"
