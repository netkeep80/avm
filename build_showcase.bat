@echo off
setlocal

cd /d "%~dp0"

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: cmake.exe was not found in PATH.
    exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
    echo ERROR: git.exe was not found in PATH.
    echo Dear ImGui and GLFW are downloaded by CMake FetchContent on the first configure.
    exit /b 1
)

set "BUILD_DIR=build-showcase"

echo.
echo === Configure AVM Showcase ===
cmake -S . -B "%BUILD_DIR%" ^
    -DAVM_BUILD_IMGUI_DEMO=ON ^
    -DAVM_BUILD_CLI=OFF ^
    -DAVM_BUILD_CORE_TESTS=OFF ^
    -DAVM_BUILD_JSON_COMPAT_TESTS=OFF ^
    -DAVM_BUILD_ANUM_ADAPTER_TESTS=OFF
if errorlevel 1 goto :fail

echo.
echo === Build avm_showcase ===
cmake --build "%BUILD_DIR%" --config Release --target avm_showcase
if errorlevel 1 goto :fail

set "SHOWCASE_EXE="
for /r "%BUILD_DIR%" %%F in (avm_showcase.exe) do (
    set "SHOWCASE_EXE=%%F"
)

if not defined SHOWCASE_EXE (
    echo.
    echo ERROR: build succeeded, but avm_showcase.exe was not found under %BUILD_DIR%.
    exit /b 1
)

echo.
echo === Run AVM Showcase ===
echo %SHOWCASE_EXE%
start "AVM Showcase" "%SHOWCASE_EXE%"
exit /b 0

:fail
echo.
echo ERROR: AVM Showcase build failed.
exit /b 1
