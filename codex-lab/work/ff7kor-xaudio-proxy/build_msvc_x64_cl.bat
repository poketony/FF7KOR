@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "OUT_DIR=%SCRIPT_DIR%build-msvc-x64-cl"
set "SRC=%SCRIPT_DIR%src\xaudio2_9redist_proxy.cpp"
set "DEF=%SCRIPT_DIR%src\xaudio2_9redist_proxy.def"

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

where cl >nul 2>nul
if errorlevel 1 (
  echo MSVC cl.exe was not found. Run this from an x64 Developer Command Prompt.
  exit /b 1
)

cl /nologo /O2 /EHsc /W4 /DWIN32_LEAN_AND_MEAN /LD "%SRC%" /Fe:"%OUT_DIR%\xaudio2_9redist.dll" /link /MACHINE:X64 /SUBSYSTEM:WINDOWS /DEF:"%DEF%"
if errorlevel 1 exit /b 1

echo Wrote "%OUT_DIR%\xaudio2_9redist.dll"
