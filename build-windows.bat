@echo off
REM Build Low Link Scope (VST3 + CLAP) on Windows.
REM Requires: Visual Studio 2022 or newer with "Desktop development with C++" (includes CMake) and Git.
REM Run from "x64 Native Tools Command Prompt for VS 2022" or a normal cmd if cmake is on PATH.

cmake -B build -A x64
if errorlevel 1 goto :fail
cmake --build build --config Release --parallel
if errorlevel 1 goto :fail

echo.
echo Hotovo. Plugin je ve slozce:
echo   build\LowLinkScope_artefacts\Release\VST3\Low Link Scope.vst3
echo Zkopiruj celou slozku "Low Link Scope.vst3" do C:\Program Files\Common Files\VST3
goto :eof

:fail
echo Build selhal.
exit /b 1
