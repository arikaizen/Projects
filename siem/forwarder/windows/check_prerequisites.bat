@echo off
REM Prerequisite Checker and Installer for Windows Event Log Forwarder
REM This script checks for required build tools and offers to install them

SETLOCAL EnableDelayedExpansion

echo ========================================
echo Windows Event Log Forwarder
echo Prerequisite Checker
echo ========================================
echo.

SET MISSING_TOOLS=0

REM Check for .NET Framework
echo [1/4] Checking for .NET Framework...
SET DOTNET_FOUND=0

REM Check registry for .NET Framework 4.x installations
reg query "HKLM\SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full" /v Version >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    for /f "tokens=3" %%i in ('reg query "HKLM\SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full" /v Version ^| findstr Version') do (
        echo   [OK] .NET Framework found: %%i
        SET DOTNET_FOUND=1
        goto :check_cmake
    )
)

if !DOTNET_FOUND! EQU 0 (
    echo   [MISSING] .NET Framework 4.0 or higher not found
    echo   [INFO] Required for installing Visual Studio
    SET MISSING_TOOLS=1
    SET MISSING_DOTNET=1
)

:check_cmake
REM Check for CMake
echo [2/4] Checking for CMake...
where cmake >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    for /f "tokens=*" %%i in ('cmake --version') do (
        echo   [OK] CMake found: %%i
        goto :check_vs
    )
) else (
    echo   [MISSING] CMake not found
    SET MISSING_TOOLS=1
    SET MISSING_CMAKE=1
)

:check_compiler
REM Check for C++ Compiler (MinGW or Visual Studio)
echo [3/4] Checking for C++ Compiler...
SET COMPILER_FOUND=0

REM Check for MinGW (recommended)
where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    for /f "tokens=*" %%i in ('g++ --version') do (
        echo   [OK] MinGW g++ found: %%i
        SET COMPILER_FOUND=1
        goto :check_winsdk
    )
)

REM Check for Visual Studio as alternative
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    echo   [OK] Visual Studio 2022 Community found
    SET COMPILER_FOUND=1
    goto :check_winsdk
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    echo   [OK] Visual Studio 2022 Professional found
    SET COMPILER_FOUND=1
    goto :check_winsdk
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" (
    echo   [OK] Visual Studio 2022 Enterprise found
    SET COMPILER_FOUND=1
    goto :check_winsdk
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    echo   [OK] Visual Studio 2019 Community found
    SET COMPILER_FOUND=1
    goto :check_winsdk
)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat" (
    echo   [OK] Visual Studio 2019 Professional found
    SET COMPILER_FOUND=1
    goto :check_winsdk
)

if !COMPILER_FOUND! EQU 0 (
    echo   [MISSING] C++ Compiler not found
    SET MISSING_TOOLS=1
    SET MISSING_COMPILER=1
)

:check_winsdk
REM Check for Windows SDK (usually comes with VS)
echo [4/4] Checking for Windows SDK...
if exist "C:\Program Files (x86)\Windows Kits\10\Include" (
    echo   [OK] Windows SDK 10 found
) else if exist "C:\Program Files (x86)\Windows Kits\8.1\Include" (
    echo   [OK] Windows SDK 8.1 found
) else (
    echo   [INFO] Windows SDK not found separately (usually included with Visual Studio)
)

echo.
echo ========================================
echo Prerequisite Check Complete
echo ========================================
echo.

if !MISSING_TOOLS! EQU 0 (
    echo [SUCCESS] All prerequisites are installed!
    echo.
    echo You can now build the forwarder by running:
    echo   build.bat
    echo.
    pause
    exit /b 0
)

REM Handle missing tools
echo [WARNING] Some prerequisites are missing
echo.

REM Note: .NET Framework only needed if installing Visual Studio (not needed for MinGW)
if DEFINED MISSING_DOTNET (
    if DEFINED MISSING_COMPILER (
        echo [INFO] .NET Framework 4.0 or higher not found
        echo   Only required if you choose to install Visual Studio
        echo   Not needed for MinGW (recommended)
        echo.
    )
)

if DEFINED MISSING_CMAKE (
    echo Missing: CMake
    echo   Download from: https://cmake.org/download/
    echo   Recommended: cmake-3.28.0-windows-x86_64.msi
    echo.
    set /p INSTALL_CMAKE="Would you like to open the CMake download page? (Y/N): "
    if /i "!INSTALL_CMAKE!"=="Y" (
        start https://cmake.org/download/
        echo   Opening browser to CMake download page...
        echo   After installing, add CMake to PATH and re-run this script
        echo.
    )
)

if DEFINED MISSING_COMPILER (
    echo Missing: C++ Compiler
    echo.
    echo Option 1 - MinGW-w64 (Recommended - Lightweight ~500MB):
    echo   Download: https://github.com/niXman/mingw-builds-binaries/releases
    echo   Get: x86_64-*-posix-seh-ucrt-*.7z (latest version)
    echo   Extract to: C:\mingw64
    echo   Add to PATH: C:\mingw64\bin
    echo.
    echo   Alternative - MSYS2 (includes package manager):
    echo   Download: https://www.msys2.org/
    echo   After install, run: pacman -S mingw-w64-x86_64-gcc
    echo.
    echo Option 2 - Visual Studio (Alternative - Large ~7GB):
    echo   Download: https://visualstudio.microsoft.com/downloads/
    echo   Requires: .NET Framework 4.8+ and "Desktop development with C++"
    echo.
    set /p INSTALL_CHOICE="Open MinGW download page? (Y/N): "
    if /i "!INSTALL_CHOICE!"=="Y" (
        start https://github.com/niXman/mingw-builds-binaries/releases
        echo   Opening browser to MinGW download page...
        echo   After installing, add to PATH and re-run this script
        echo.
    )
)

echo ========================================
echo Installation Instructions
echo ========================================
echo.
echo After installing the missing tools:
echo   1. Restart your command prompt
echo   2. Run this script again to verify
echo   3. Run build.bat to compile the forwarder
echo.
pause
exit /b 1
