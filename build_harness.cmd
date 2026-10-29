@echo off
setlocal
cd /d "%~dp0"

set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% ( echo vswhere.exe not found & exit /b 1 )

for /f "usebackq tokens=*" %%i in (
    `%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set VS_PATH=%%i
if "%VS_PATH%"=="" ( echo VS C++ tools not found & exit /b 1 )

:: "build_harness.cmd x86" builds a 32-bit harness, to run the tests as the Win32 plugin would.
set "VCARG=x64"
if /i "%1"=="x86" set "VCARG=x64_x86"
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" %VCARG% >nul 2>&1

mkdir build\harness 2>nul

cl /nologo /O2 /EHsc /std:c++17 /utf-8 ^
   src\test_harness.cpp src\formatter.cpp ^
   /Fe:build\harness\test_harness.exe ^
   /Fo:build\harness\
if errorlevel 1 ( echo Build failed & exit /b 1 )

cl /nologo /O2 /EHsc /std:c++17 /utf-8 ^
   src\test_dialects.cpp src\dialects.cpp src\formatter.cpp ^
   /Fe:build\harness\test_dialects.exe ^
   /Fo:build\harness\
if errorlevel 1 ( echo Build failed & exit /b 1 )

echo Harness build complete.
exit /b 0
