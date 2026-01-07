@echo off
REM ============================================================================
REM Windows Event Log Forwarder - Build Script
REM ============================================================================
REM
REM This script compiles the Windows Event Log Forwarder using g++.
REM
REM Requirements:
REM   - MinGW-w64 with g++ compiler and Windows SDK headers
REM
REM Usage:
REM   Simply run: build.bat
REM
REM Output:
REM   - bin/log_forwarder.exe  (Main forwarder application)
REM ============================================================================

echo ====================================
echo Windows Event Log Forwarder Builder
echo ====================================
echo.

REM ============================================================================
REM STEP 1: Verify g++ Compiler
REM ============================================================================
echo [1/3] Checking for g++ compiler...
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: g++ compiler not found!
    echo ============================================================
    echo.
    echo This project requires MinGW-w64 with g++ compiler.
    echo.
    echo SOLUTION:
    echo   1. Download MinGW-w64 from: https://github.com/niXman/mingw-builds-binaries/releases
    echo   2. Get file: x86_64-*-release-posix-seh-ucrt-*.7z
    echo   3. Extract to C:\mingw64
    echo   4. Add C:\mingw64\bin to your PATH environment variable
    echo   5. Restart terminal and run build.bat again
    echo.
    pause
    exit /b 1
)
echo       [OK] g++ compiler found
echo.

REM ============================================================================
REM STEP 2: Create bin Directory
REM ============================================================================
echo [2/3] Preparing output directory...
if not exist bin mkdir bin
echo       [OK] bin directory ready
echo.

REM ============================================================================
REM STEP 3: Compile the Project
REM ============================================================================
echo [3/3] Compiling project...
echo       Running: g++ -I./inc src/main.cpp src/log_forwarder.cpp src/event_log_reader.cpp src/json_utils.cpp src/forwarder_api.cpp -o bin/log_forwarder.exe -lwevtapi -lws2_32 -std=c++17
echo.

g++ -I./inc src/main.cpp src/log_forwarder.cpp src/event_log_reader.cpp src/json_utils.cpp src/forwarder_api.cpp -o bin/log_forwarder.exe -lwevtapi -lws2_32 -std=c++17

REM Check if compilation succeeded
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: Build failed
    echo ============================================================
    echo.
    echo The compilation process encountered errors.
    echo.
    echo COMMON ISSUES:
    echo   - Missing Windows SDK headers (winevt.h, winsock2.h)
    echo   - Syntax errors in source code
    echo   - Missing libraries (ws2_32.lib, wevtapi.lib)
    echo   - You need MinGW-w64, NOT standard MinGW
    echo.
    echo Check the error messages above for specific details.
    echo.
    pause
    exit /b 1
)

REM ============================================================================
REM SUCCESS: Build Complete
REM ============================================================================
echo.
echo ============================================================
echo BUILD SUCCESSFUL!
echo ============================================================
echo.
echo Output file:
echo   bin\log_forwarder.exe  - Main application
echo.
echo NEXT STEPS:
echo.
echo 1. Run the forwarder:
echo    bin\log_forwarder.exe
echo.
echo 2. Connect to SIEM server:
echo    bin\log_forwarder.exe [server_ip] [port]
echo    Example: bin\log_forwarder.exe 192.168.1.100 8089
echo.
echo 3. Run as Administrator (required for Security event logs):
echo    Right-click bin\log_forwarder.exe -^> Run as administrator
echo.
echo ============================================================
echo.

pause
