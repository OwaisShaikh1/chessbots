@echo off
echo Building Chess C++ Backend...

if not exist build mkdir build
cd build

echo Configuring with CMake...
cmake .. -G "Visual Studio 17 2022" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed. Trying MinGW...
    cd ..
    rmdir /s /q build
    mkdir build
    cd build
    cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
)

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed.
    pause
    exit /b 1
)

echo Building...
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo Build failed.
    pause
    exit /b 1
)

echo.
echo Build successful!
echo Executable located at: build\bin\Release\chess_backend.exe (Visual Studio)
echo                    or: build\bin\chess_backend.exe (MinGW)
echo.
echo To run: chess_backend.exe [port]
echo Default port: 8000
pause
