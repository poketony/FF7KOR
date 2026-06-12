@echo off
setlocal
cd /d "%~dp0"
cmake -S . -B build-msvc-win32 -A Win32
if errorlevel 1 exit /b %errorlevel%
cmake --build build-msvc-win32 --config Release
if errorlevel 1 exit /b %errorlevel%
echo Built: %~dp0build-msvc-win32\Release\ff7kor.dll
