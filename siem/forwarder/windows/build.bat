@echo off
echo ====================================
echo Windows Event Log Forwarder Builder
echo ====================================
echo.

REM Check for compiler
echo Checking for C++ compiler...
where cl.exe >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo [OK] Detected Visual Studio compiler
    set COMPILER=MSVC
    goto :build
)

where g++.exe >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo [OK] Detected MinGW g++ compiler
    set COMPILER=MinGW
    goto :build
)

echo [ERROR] No C++ compiler found!
echo.
echo Please install one of the following:
echo   - Visual Studio (with C++ tools)
echo   - MinGW-w64
echo.
pause
exit /b 1

:build
echo Using compiler: %COMPILER%
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Generate build files
echo Generating build files with %COMPILER%...
if "%COMPILER%"=="MinGW" (
    cmake -G "MinGW Makefiles" ..
) else (
    cmake ..
)

if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed
    cd ..
    pause
    exit /b 1
)

echo.
echo Building project with %COMPILER%...
if "%COMPILER%"=="MinGW" (
    mingw32-make
) else (
    cmake --build . --config Release
)

if %ERRORLEVEL% neq 0 (
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

cd ..

echo.
echo ============================================================
echo Build completed successfully!
echo ============================================================
echo.
echo Executable location: bin\log_forwarder.exe
echo Test executable: bin\test_forwarder.exe
echo.
echo To run the forwarder:
echo   bin\log_forwarder.exe ^<server^> ^<port^> [channel] [interval]
echo.
echo Example:
echo   bin\log_forwarder.exe 192.168.1.100 5000 Security 30
echo.
pause
