@echo off
setlocal EnableExtensions
cd /d "%~dp0"

where cl >nul 2>nul
if errorlevel 1 (
    call :find_msvc
)

where cl >nul 2>nul
if errorlevel 1 (
    echo MSVC cl.exe was not found. Install Visual Studio Build Tools with x86/x64 C++ tools, or run this from an x86 Developer Command Prompt.
    exit /b 1
)

if not exist build-msvc-win32-cl mkdir build-msvc-win32-cl

where cl
where link
cl /nologo /std:c++17 /EHsc /O2 /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /LD src\ff7kor.cpp /Fobuild-msvc-win32-cl\ /Febuild-msvc-win32-cl\ff7kor.dll /link /DYNAMICBASE:NO /BASE:0x10000000 /IMPLIB:build-msvc-win32-cl\ff7kor.lib
if errorlevel 1 exit /b %errorlevel%

echo Built: %~dp0build-msvc-win32-cl\ff7kor.dll
exit /b 0

:find_msvc
echo cl.exe is not currently on PATH. Searching for Visual Studio...

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
    "%VSWHERE%" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%I"
)

if not defined VSINSTALL (
    for %%D in (
        "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
        "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Enterprise"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Professional"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
    ) do (
        if exist "%%~D\VC\Auxiliary\Build\vcvarsall.bat" set "VSINSTALL=%%~D"
    )
)

if defined VSINSTALL (
    echo Using Visual Studio at: %VSINSTALL%
    call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86
) else (
    echo Visual Studio with x86 C++ tools was not found.
)
exit /b 0
