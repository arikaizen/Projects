@echo off
REM Automatic compilation script for Windows Event Log Forwarder

echo ====================================
echo Windows Event Log Forwarder Builder
echo ====================================
echo.

REM Check if CMake is installed
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake is not installed or not in PATH
    echo Please install CMake from https://cmake.org/download/
    pause
    exit /b 1
)

REM Detect available compiler
SET COMPILER_TYPE=unknown
SET CMAKE_GENERATOR=

REM Check for MinGW (preferred)
where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo [OK] Detected MinGW g++ compiler
    SET COMPILER_TYPE=MinGW
    SET CMAKE_GENERATOR=-G "MinGW Makefiles"
    goto :found_compiler
)

REM Check for Visual Studio 2022
if exist "C:\Program Files\Microsoft Visual Studio\2022" (
    echo [OK] Detected Visual Studio 2022
    SET COMPILER_TYPE=VS2022
    SET CMAKE_GENERATOR=-G "Visual Studio 17 2022" -A x64
    goto :found_compiler
)

REM Check for Visual Studio 2019
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019" (
    echo [OK] Detected Visual Studio 2019
    SET COMPILER_TYPE=VS2019
    SET CMAKE_GENERATOR=-G "Visual Studio 16 2019" -A x64
    goto :found_compiler
)

echo ERROR: No C++ compiler found
echo Please install MinGW or Visual Studio
echo Run check_prerequisites.bat for help
pause
exit /b 1

:found_compiler
echo Using compiler: %COMPILER_TYPE%
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Generate build files
echo Generating build files...
cmake .. %CMAKE_GENERATOR%
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build the project
echo.
echo Building project...
if "%COMPILER_TYPE%"=="MinGW" (
    mingw32-make -j4
) else (
    cmake --build . --config Release
)
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

echo.
echo ====================================
echo Build completed successfully!
echo Executable: bin\log_forwarder.exe
echo ====================================
echo.

cd ..
pause
