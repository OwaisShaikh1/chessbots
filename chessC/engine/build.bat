@echo off
setlocal
setlocal EnableDelayedExpansion

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build
set BOTS_DIR=%SCRIPT_DIR%..\bots
set GENERATOR=
set VERSION_NAME=%~1

where cmake >nul 2>nul
if errorlevel 1 (
	echo ERROR: cmake not found in PATH.
	echo Install CMake and restart the terminal.
	exit /b 1
)

where ninja >nul 2>nul
if not errorlevel 1 set "GENERATOR=Ninja"

if not defined GENERATOR (
	where mingw32-make >nul 2>nul
	if not errorlevel 1 set "GENERATOR=MinGW Makefiles"
)

if not defined GENERATOR (
	where nmake >nul 2>nul
	if not errorlevel 1 set "GENERATOR=NMake Makefiles"
)

if not defined GENERATOR (
	echo ERROR: No supported build tool found.
	echo Install one of the following and reopen terminal:
	echo   - Ninja ^(recommended^)
	echo   - MinGW ^(mingw32-make + g++^)
	echo   - Visual Studio Build Tools ^(nmake + cl in Developer Prompt^)
	where scoop >nul 2>nul
	if not errorlevel 1 (
		echo.
		echo Detected Scoop. Quick setup command:
		echo   scoop install gcc ninja cmake
	)
	exit /b 1
)

echo Using CMake generator: %GENERATOR%

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"

set CACHE_GENERATOR=
if exist "CMakeCache.txt" (
	for /f "tokens=2 delims==" %%G in ('findstr /b "CMAKE_GENERATOR:INTERNAL=" "CMakeCache.txt"') do set "CACHE_GENERATOR=%%G"
	if defined CACHE_GENERATOR if not "%CACHE_GENERATOR%"=="" if /I not "%CACHE_GENERATOR%"=="%GENERATOR%" (
		popd
		echo Existing build directory uses "%CACHE_GENERATOR%"; recreating for "%GENERATOR%".
		rmdir /s /q "%BUILD_DIR%"
		mkdir "%BUILD_DIR%"
		pushd "%BUILD_DIR%"
	)
)

cmake .. -G "%GENERATOR%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
	popd
	echo ERROR: CMake configure failed.
	exit /b 1
)

cmake --build . --config Release
if errorlevel 1 (
	popd
	echo ERROR: Build failed.
	exit /b 1
)

popd

if exist "%BUILD_DIR%\Release\chessbot.exe" (
	echo.
	echo Build successful! Binary: %BUILD_DIR%\Release\chessbot.exe
	set BUILT_BIN=%BUILD_DIR%\Release\chessbot.exe
	goto :post_build
	exit /b 0
)

if exist "%BUILD_DIR%\Release\chessbot" (
	echo.
	echo Build successful! Binary: %BUILD_DIR%\Release\chessbot
	set BUILT_BIN=%BUILD_DIR%\Release\chessbot
	goto :post_build
	exit /b 0
)

if exist "%BUILD_DIR%\chessbot.exe" (
	echo.
	echo Build successful! Binary: %BUILD_DIR%\chessbot.exe
	set BUILT_BIN=%BUILD_DIR%\chessbot.exe
	goto :post_build
	exit /b 0
)

if exist "%BUILD_DIR%\chessbot" (
	echo.
	echo Build successful! Binary: %BUILD_DIR%\chessbot
	set BUILT_BIN=%BUILD_DIR%\chessbot
	goto :post_build
	exit /b 0
)

echo ERROR: Build completed but chessbot binary was not found.
exit /b 1

:post_build
if not defined VERSION_NAME (
	exit /b 0
)

if /I "%VERSION_NAME%"=="auto" goto :auto_disabled
if /I "%VERSION_NAME%"=="next" goto :auto_disabled

goto :validate_version

:auto_disabled
echo ERROR: Auto version naming is disabled to prevent accidental overwrite.
echo Use an explicit version name, for example:
echo   .\build.bat v003_minimax_tuned
exit /b 1

:validate_version

powershell -NoProfile -Command "$n='%VERSION_NAME%'; if($n -match '^[A-Za-z0-9_.-]+$'){ exit 0 } else { exit 1 }" >nul 2>nul
if errorlevel 1 (
	echo ERROR: Invalid version name "%VERSION_NAME%".
	echo Use only letters, numbers, dot, underscore, or dash.
	exit /b 1
)

set TARGET_DIR=%BOTS_DIR%\%VERSION_NAME%
if not exist "%TARGET_DIR%" mkdir "%TARGET_DIR%"

copy /y "%BUILT_BIN%" "%TARGET_DIR%\" >nul
if errorlevel 1 (
	echo ERROR: Failed to copy engine binary to "%TARGET_DIR%".
	exit /b 1
)

echo Saved bot version "%VERSION_NAME%" to: %TARGET_DIR%
exit /b 0
