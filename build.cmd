@echo off
:: Builds three artifacts from the same formatter engine: the NPP plugin DLL,
:: the standalone CLI (build\cli\formatsql.exe), and (if em++ is on PATH) the
:: WASM module for the website playground - published to C:\Git\datamodder\public\.
setlocal
cd /d "%~dp0"

:: -- Find Visual Studio --
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% ( echo vswhere.exe not found & exit /b 1 )

for /f "usebackq tokens=*" %%i in (
    `%VSWHERE% -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set VS_PATH=%%i
if "%VS_PATH%"=="" ( echo VS C++ tools not found & exit /b 1 )

call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

mkdir build 2>nul

:: -- Compile resource --
rc.exe /nologo /fo build\settings.res src\settings.rc
if errorlevel 1 ( echo Resource compile failed & exit /b 1 )

:: -- Compile and link --
cl /LD /O2 /EHsc /std:c++17 /MT /utf-8 ^
   src\dllmain.cpp src\formatter.cpp src\settings_dialog.cpp src\dialects.cpp src\settings_io.cpp ^
   build\settings.res ^
   /Fe:build\FormatSQL.dll ^
   /Fo:build\ ^
   /link user32.lib comctl32.lib comdlg32.lib shell32.lib
if errorlevel 1 ( echo Build failed & exit /b 1 )

:: -- Close Notepad++ --
taskkill /f /im notepad++.exe >nul 2>&1
ping -n 2 127.0.0.1 >nul 2>&1

:: -- Deploy DLL --
set DST=C:\Program Files\Notepad++\plugins\FormatSQL
if not exist "%DST%" mkdir "%DST%"
copy /y "build\FormatSQL.dll" "%DST%\FormatSQL.dll"
if errorlevel 1 ( echo Copy failed & exit /b 1 )
copy /y "src\help.txt" "%DST%\help.txt"
if errorlevel 1 ( echo Help file copy failed & exit /b 1 )

:: -- Restart Notepad++ --
if exist "C:\Program Files\Notepad++\notepad++.exe" ( start "" "C:\Program Files\Notepad++\notepad++.exe" )

:: -- Build the standalone CLI (same engine, no NPP/WinAPI-dialog dependency) --
mkdir build\cli 2>nul
cl /nologo /O2 /EHsc /std:c++17 /utf-8 ^
   src\cli_main.cpp src\formatter.cpp src\dialects.cpp src\settings_io.cpp ^
   /Fe:build\cli\formatsql.exe /Fo:build\cli\
if errorlevel 1 ( echo CLI build failed - DLL was already deployed successfully. & exit /b 1 )

for /f "usebackq skip=1 tokens=*" %%H in (`certutil -hashfile build\cli\formatsql.exe SHA256 ^| findstr /v "CertUtil"`) do (
    echo %%H> build\cli\formatsql.sha256.txt
    goto :hash_done
)
:hash_done
build\cli\formatsql.exe --version > build\cli\formatsql.version.txt

:: -- Build the WASM module for the website playground (optional, needs emsdk) --
where em++ >nul 2>&1
if errorlevel 1 (
    echo em++ not found on PATH - WASM build skipped. Run emsdk_env.bat in this shell to include it.
) else (
    mkdir build\wasm 2>nul
    em++ -O2 -std=c++17 src\wasm_bindings.cpp src\formatter.cpp src\dialects.cpp src\settings_io.cpp -I src ^
         -s WASM=1 -s MODULARIZE=1 -s EXPORT_NAME=FormatSQLModule ^
         -s EXPORTED_FUNCTIONS=_formatsql_format,_formatsql_minify,_formatsql_convert_dialect,_formatsql_load_settings_ini,_formatsql_export_settings_ini,_formatsql_reset_settings,_formatsql_free,_malloc,_free ^
         -s EXPORTED_RUNTIME_METHODS=ccall,cwrap,UTF8ToString ^
         -s ALLOW_MEMORY_GROWTH=1 -s ENVIRONMENT=web -s NO_EXIT_RUNTIME=1 ^
         -o build\wasm\formatsql.js
    if errorlevel 1 ( echo WASM build failed - DLL and CLI are already built/deployed. ) else ( echo WASM build OK. )
)

:: -- Publish the CLI + WASM artifacts to the website repo, if present --
if exist "C:\Git\datamodder" (
    if not exist "C:\Git\datamodder\public\downloads\formatsql" mkdir "C:\Git\datamodder\public\downloads\formatsql"
    copy /y build\cli\formatsql.exe          "C:\Git\datamodder\public\downloads\formatsql\formatsql.exe"
    copy /y build\cli\formatsql.sha256.txt   "C:\Git\datamodder\public\downloads\formatsql\formatsql.sha256.txt"
    copy /y build\cli\formatsql.version.txt  "C:\Git\datamodder\public\downloads\formatsql\formatsql.version.txt"
    if exist build\wasm\formatsql.wasm (
        if not exist "C:\Git\datamodder\public\wasm\formatsql" mkdir "C:\Git\datamodder\public\wasm\formatsql"
        copy /y build\wasm\formatsql.js   "C:\Git\datamodder\public\wasm\formatsql\formatsql.js"
        copy /y build\wasm\formatsql.wasm "C:\Git\datamodder\public\wasm\formatsql\formatsql.wasm"
    )
    echo Copied to C:\Git\datamodder\public\ - commit + deploy that repo to go live.
)

echo Build and deploy complete.
exit /b 0
