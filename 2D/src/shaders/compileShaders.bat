@echo off

set GLSLC=glslc.exe
set SHADERS_FOLDER=%CD%
set SPIRV_FOLDER=%CD%\spv

for /r "%SHADERS_FOLDER%" %%f in (*.vert) do (
    %GLSLC% "%%f" --target-env=vulkan1.2 -o "%SPIRV_FOLDER%\%%~nf.vert.spv"
)

for /r "%SHADERS_FOLDER%" %%f in (*.frag) do (
    %GLSLC% "%%f" --target-env=vulkan1.2 -o "%SPIRV_FOLDER%\%%~nf.frag.spv"
)

for /r "%SHADERS_FOLDER%" %%f in (*.rchit) do (
    %GLSLC% "%%f" --target-env=vulkan1.2 -o "%SPIRV_FOLDER%\%%~nf.rchit.spv"
)

for /r "%SHADERS_FOLDER%" %%f in (*.rmiss) do (
    %GLSLC% "%%f" --target-env=vulkan1.2 -o "%SPIRV_FOLDER%\%%~nf.rmiss.spv"
)

for /r "%SHADERS_FOLDER%" %%f in (*.rgen) do (
    %GLSLC% "%%f" --target-env=vulkan1.2 -o "%SPIRV_FOLDER%\%%~nf.rgen.spv"
)

for /r "%SHADERS_FOLDER%" %%f in (*.comp) do (
    %GLSLC% "%%f" --target-env=vulkan1.2 -o "%SPIRV_FOLDER%\%%~nf.comp.spv"
)

PAUSE