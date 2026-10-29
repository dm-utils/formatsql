@echo off
setlocal
cd /d "%~dp0"

:: Builds FormatSQL.dll for each Notepad++ flavour (x64, x86 = Win32, arm64)
:: and zips it with help.txt into release_staging\FormatSQL.zip (x64) or
:: FormatSQL_<arch>.zip, for the GitHub release and the plugin lists (pl.x64.json, pl.x86.json, pl.arm64.json).
:: Prints the SHA-256 of each zip, which the plugin lists use as "id".
::   release.cmd              x64 only (see below to build all three)
::   release.cmd x64 x86 arm64  these

:: -- Find Visual Studio --
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% ( echo vswhere.exe not found & exit /b 1 )
for /f "usebackq tokens=*" %%i in (
    `%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set VS_PATH=%%i
if "%VS_PATH%"=="" ( echo VS C++ tools not found & exit /b 1 )


set "ARCHS=%*"
if "%ARCHS%"=="" set "ARCHS=x64"
:: Win32 and ARM64 Notepad++ are rare; uncomment to build those as well.
:: if "%ARCHS%"=="x64" set "ARCHS=x64 x86 arm64"
mkdir release_staging 2>nul

for %%A in (%ARCHS%) do (
    call :build %%A
    if errorlevel 1 exit /b 1
)
echo.
echo Release zips in release_staging\ - SHA-256 for the plugin lists:
for %%A in (%ARCHS%) do (
    set "ZIP=FormatSQL_%%A.zip"
    if "%%A"=="x64" set "ZIP=FormatSQL.zip"
    call :hash %%A
)
exit /b 0

:hash
for /f "skip=1 tokens=*" %%h in ('certutil -hashfile "release_staging\%ZIP%" SHA256 ^| findstr /v "CertUtil"') do echo   %1  %ZIP%  %%h
exit /b 0

:: -- One architecture: compile with the matching (cross) compiler, then zip --
:build
setlocal
set "ARCH=%1"
if "%ARCH%"=="x64" ( set "VCARG=x64" ) else if "%ARCH%"=="x86" ( set "VCARG=x64_x86" ) else if "%ARCH%"=="arm64" ( set "VCARG=x64_arm64" ) else (
    echo Unknown architecture %ARCH%; use x64, x86 or arm64.
    exit /b 1
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" %VCARG% >nul 2>&1
if not "%VSCMD_ARG_TGT_ARCH%"=="%ARCH%" (
    echo No C++ compiler for %ARCH%. In the Visual Studio Installer, add
    echo "MSVC C++ ARM64/ARM64EC build tools" ^(for arm64^) and try again.
    exit /b 1
)
echo === %ARCH% ===
set "OUT=build\%ARCH%"
mkdir "%OUT%" 2>nul
rc.exe /nologo /fo "%OUT%\settings.res" src\settings.rc
if errorlevel 1 ( echo Resource compile failed & exit /b 1 )
cl /nologo /LD /O2 /EHsc /std:c++17 /MT /utf-8 /W3 ^
   src\dllmain.cpp src\formatter.cpp src\settings_dialog.cpp src\dialects.cpp src\settings_io.cpp ^
   "%OUT%\settings.res" ^
   /Fe:"%OUT%\FormatSQL.dll" ^
   /Fo:"%OUT%\\" ^
   /link user32.lib comctl32.lib comdlg32.lib shell32.lib
if errorlevel 1 ( echo Build failed for %ARCH% & exit /b 1 )
set "ZIP=FormatSQL_%ARCH%.zip"
if "%ARCH%"=="x64" set "ZIP=FormatSQL.zip"
del /q "release_staging\%ZIP%" 2>nul
powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\FormatSQL.dll','src\help.txt' -DestinationPath 'release_staging\%ZIP%'"
if errorlevel 1 ( echo Zip failed for %ARCH% & exit /b 1 )
endlocal
exit /b 0
