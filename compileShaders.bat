@echo off

set GLSLC=glslc.exe

for %%f in (*.vert *.frag *.rchit *.rmiss *.rgen *.comp) do (
    set SHADER_PATH=%CD%
    set SPIRV_FOLDER=%SHADER_PATH%\spv

    if not exist "%SPIRV_FOLDER%" mkdir "%SPIRV_FOLDER%"
    
    %GLSLC% "%%f" --target-env=vulkan1.2 -o "%SPIRV_FOLDER%\%%~nf.spv"
)

PAUSE