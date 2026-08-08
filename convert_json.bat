@echo off
setlocal

cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: cmake.exe was not found in PATH.
    exit /b 1
)

set "BUILD_DIR=build"
set "CONVERTER_EXE="

for /r "%BUILD_DIR%" %%F in (avm-json-convert.exe) do (
    set "CONVERTER_EXE=%%F"
)

if not defined CONVERTER_EXE (
    echo.
    echo === Build avm-json-convert ===
    cmake -S . -B "%BUILD_DIR%"
    if errorlevel 1 goto :fail

    cmake --build "%BUILD_DIR%" --config Release --target avm_json_convert
    if errorlevel 1 goto :fail

    for /r "%BUILD_DIR%" %%F in (avm-json-convert.exe) do (
        set "CONVERTER_EXE=%%F"
    )
)

if not defined CONVERTER_EXE (
    echo ERROR: avm-json-convert.exe was not found under %BUILD_DIR%.
    exit /b 1
)

echo.
echo === AVM JSON converter ===
echo %CONVERTER_EXE%
"%CONVERTER_EXE%" %*
exit /b %errorlevel%

:fail
echo.
echo ERROR: avm-json-convert build failed.
exit /b 1
