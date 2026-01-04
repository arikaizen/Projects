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

REM Create build directory
if not exist build mkdir build
cd build

REM Generate build files
echo Generating build files...
cmake .. -G "Visual Studio 16 2019" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

REM Build the project
echo.
echo Building project...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

echo.
echo ====================================
echo Build completed successfully!
echo Executable: build\bin\Release\log_forwarder.exe
echo ====================================
echo.

cd ..
pause
