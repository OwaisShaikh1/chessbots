@echo off
setlocal

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"

cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release

popd
echo.
echo Build successful! Binary: %BUILD_DIR%\Release\chessbot.exe
