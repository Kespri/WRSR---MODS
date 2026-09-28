@echo off
setlocal
pushd "%~dp0"
where cl >nul 2>nul
if errorlevel 1 (
    set "RP_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" goto no_compiler
    for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do call "%%I\VC\Auxiliary\Build\vcvars64.bat" >nul
)
where cl >nul 2>nul
if errorlevel 1 goto no_compiler
if not exist build mkdir build
cl /nologo /std:c++17 /utf-8 /O2 /MT /W4 /wd4505 /EHsc /LD /Fo"build\rail_physics_fix.obj" /Fe"build\rail_physics_fix.dll" rail_physics_fix.cpp /link kernel32.lib /INCREMENTAL:NO
if errorlevel 1 goto failed
copy /y rail_physics_fix.ini build\rail_physics_fix.ini >nul
echo Built build\rail_physics_fix.dll. No game files installed or modified.
popd
exit /b 0
:no_compiler
echo ERROR: Visual Studio x64 C++ build tools are required.
:failed
popd
exit /b 1
