@echo off
REM Automatic compilation script for Windows Event Log Forwarder

echo ====================================
echo Windows Event Log Forwarder Builder
echo ====================================
echo.

REM Check if CMake is installed
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: CMake is not installed or not in PATH
    echo ============================================================
    echo.
    echo CMake is required to build this project.
    echo.
    echo TROUBLESHOOTING:
    echo   1. Check if CMake is installed: where cmake
    echo   2. If installed, add CMake to your PATH environment variable
    echo   3. If not installed, download from: https://cmake.org/download/
    echo.
    echo Run 'install_prerequisites.ps1' for automatic installation
    echo.
    pause
    exit /b 1
)

REM Detect available compiler
echo Checking for C++ compiler...
echo.
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

echo.
echo ============================================================
echo ERROR: No C++ compiler found!
echo ============================================================
echo.
echo This project requires either MinGW or Visual Studio to compile.
echo.
echo TROUBLESHOOTING:
echo.
echo If you have MinGW installed:
echo   1. Check if g++ is in your PATH: where g++
echo   2. If not found, add MinGW bin folder to PATH
echo      Example: C:\mingw64\bin
echo   3. Restart your command prompt after changing PATH
echo   4. Run this script again
echo.
echo If you don't have MinGW installed:
echo   Option 1 - Install MinGW (Recommended - ~500MB):
echo     - Download: https://github.com/niXman/mingw-builds-binaries/releases
echo     - Extract to C:\mingw64
echo     - Add C:\mingw64\bin to your PATH
echo.
echo   Option 2 - Install Visual Studio (~7GB):
echo     - Download: https://visualstudio.microsoft.com/downloads/
echo     - Install "Desktop development with C++" workload
echo.
echo For automatic setup, run: install_prerequisites.ps1
echo.
pause
exit /b 1

:found_compiler
echo Using compiler: %COMPILER_TYPE%
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Generate build files
echo Generating build files with %COMPILER_TYPE%...
cmake .. %CMAKE_GENERATOR%
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: CMake configuration failed
    echo ============================================================
    echo.
    echo CMake was unable to generate build files.
    echo.
    echo POSSIBLE CAUSES:
    echo   - Missing compiler tools
    echo   - CMakeLists.txt errors
    echo   - Missing dependencies
    echo.
    echo Check the error messages above for details.
    echo.
    cd ..
    pause
    exit /b 1
)

REM Build the project
echo.
echo Building project with %COMPILER_TYPE%...
if "%COMPILER_TYPE%"=="MinGW" (
    mingw32-make -j4
) else (
    cmake --build . --config Release
)
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: Build failed
    echo ============================================================
    echo.
    echo The compilation process encountered errors.
    echo.
    echo COMMON ISSUES:
    echo   - Missing Windows SDK (for Visual Studio builds)
    echo   - Syntax errors in source code
    echo   - Missing libraries or headers
    echo.
    echo Check the error messages above for specific details.
    echo.
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
