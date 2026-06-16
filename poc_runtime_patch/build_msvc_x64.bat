@echo off
setlocal
cd /d "%~dp0"

where cl.exe >nul 2>nul
if %ERRORLEVEL%==0 goto build

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo cl.exe was not found, and vswhere.exe is not installed.
  echo Install Visual Studio Build Tools with the MSVC x64 toolchain, or run this from a Developer Command Prompt.
  exit /b 1
)

for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
  echo Visual Studio with MSVC x64 tools was not found.
  exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

:build
cl /nologo /std:c++17 /EHsc /W4 /O2 /DNDEBUG /DUNICODE /D_UNICODE c0_poc_patcher.cpp /Fe:c0_poc_patcher.exe
exit /b %ERRORLEVEL%
