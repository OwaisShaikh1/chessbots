@echo off
REM Chess C++ Backend - Windows Build Script
REM No external dependencies required

echo ========================================
echo Chess C++ Backend - Build Script
echo ========================================

REM Check for MSVC
where cl >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Using MSVC compiler...
    echo.
    cl /EHsc /O2 /std:c++17 main.cpp chess.cpp /Fe:chess_backend.exe /link ws2_32.lib
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo ========================================
        echo Build successful! Run: chess_backend.exe
        echo ========================================
    ) else (
        echo Build failed with MSVC
    )
    goto :end
)

REM Check for g++ (MinGW)
where g++ >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Using MinGW g++ compiler...
    echo.
    g++ -O2 -std=c++17 main.cpp chess.cpp -o chess_backend.exe -lws2_32 -lpthread
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo ========================================
        echo Build successful! Run: chess_backend.exe
        echo ========================================
    ) else (
        echo Build failed with g++
    )
    goto :end
)

REM Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Using Clang++ compiler...
    echo.
    clang++ -O2 -std=c++17 main.cpp chess.cpp -o chess_backend.exe -lws2_32 -lpthread
    if %ERRORLEVEL% EQU 0 (
        echo.
        echo ========================================
        echo Build successful! Run: chess_backend.exe
        echo ========================================
    ) else (
        echo Build failed with clang++
    )
    goto :end
)

echo ERROR: No C++ compiler found!
echo.
echo Please install one of:
echo   1. Visual Studio with C++ workload (recommended)
echo      - Run from "Developer Command Prompt for VS"
echo   2. MinGW-w64 with g++
echo   3. LLVM/Clang

:end
