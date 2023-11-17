@echo off

set GLSLC=glslc.exe
set SHADERS_FOLDER=Vulture\src\Vulture\Shaders
set SPIRV_FOLDER=Vulture\src\Vulture\Shaders\spv

for /r "%SHADERS_FOLDER%" %%f in (*.vert) do (
    %GLSLC% "%%f" -o "%SPIRV_FOLDER%\%%~nf.vert.spv"
)

for /r "%SHADERS_FOLDER%" %%f in (*.frag) do (
    %GLSLC% "%%f" -o "%SPIRV_FOLDER%\%%~nf.frag.spv"
)

for /r "%SHADERS_FOLDER%" %%f in (*.comp) do (
    %GLSLC% "%%f" -o "%SPIRV_FOLDER%\%%~nf.comp.spv"
)

PAUSE