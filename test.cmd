@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

call .\build_harness.cmd %1
if errorlevel 1 exit /b 1

set FAIL=0

for %%P in (default alt) do (
    .\build\harness\test_harness.exe tests\basic_test_queries.sql %%P > tests\actual_%%P.txt
    fc /a tests\golden_%%P.txt tests\actual_%%P.txt >nul
    if errorlevel 1 (
        echo MISMATCH: profile %%P differs from tests\golden_%%P.txt
        echo   run:  fc tests\golden_%%P.txt tests\actual_%%P.txt
        set FAIL=1
    ) else (
        echo OK: profile %%P matches golden output
    )
)

if !FAIL! neq 0 (
    echo.
    echo One or more profiles regressed. If the new output is correct,
    echo review it and copy tests\actual_*.txt over tests\golden_*.txt.
    exit /b 1
)

.\build\harness\test_dialects.exe
if errorlevel 1 (
    echo.
    echo Dialect conversion tests failed.
    exit /b 1
)

echo.
echo All formatter and dialect regression tests passed.
exit /b 0
