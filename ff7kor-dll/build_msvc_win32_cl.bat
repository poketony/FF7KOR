@echo off
setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if errorlevel 1 (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "%VSWHERE%" (
        for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
    )
    if defined VSINSTALL (
        call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86
    )
)

where cl >nul 2>nul
if errorlevel 1 (
    echo MSVC cl.exe was not found. Install Visual Studio Build Tools with x86/x64 C++ tools, or run this from an x86 Developer Command Prompt.
    exit /b 1
)

if not exist build-msvc-win32-cl mkdir build-msvc-win32-cl

cl /nologo /std:c++17 /EHsc /O2 /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /LD src\ff7kor.cpp /Fobuild-msvc-win32-cl\ /Febuild-msvc-win32-cl\ff7kor.dll /link /DYNAMICBASE:NO /BASE:0x10000000 /IMPLIB:build-msvc-win32-cl\ff7kor.lib
if errorlevel 1 exit /b %errorlevel%

echo Built: %~dp0build-msvc-win32-cl\ff7kor.dll
