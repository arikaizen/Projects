@echo off
REM ========================================
REM Build script for Real-Time Console Test
REM ========================================

echo ========================================
echo Building Real-Time Console Test Program
echo ========================================
echo.

REM Check if MinGW-w64 is in PATH
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] g++ not found in PATH
    echo [ERROR] Please install MinGW-w64 and add it to your PATH
    echo [ERROR] Example: set PATH=C:\msys64\mingw64\bin;%%PATH%%
    exit /b 1
)

echo [1/3] Checking directories...
if not exist "..\bin" (
    echo Creating bin directory...
    mkdir "..\bin"
)

echo.
echo [2/3] Compiling test_realtime_console.cpp...
g++ -I../inc ^
    test_realtime_console.cpp ^
    ../src/event_log_reader.cpp ^
    ../src/json_utils.cpp ^
    ../src/logger.cpp ^
    -o ../bin/test_realtime_console.exe ^
    -lwevtapi ^
    -std=c++17 ^
    -Wall

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed
    exit /b 1
)

echo.
echo [3/3] Build complete!
echo ========================================
echo.
echo Executable created: ..\bin\test_realtime_console.exe
echo.
echo Usage Examples:
echo   ..\bin\test_realtime_console.exe
echo   ..\bin\test_realtime_console.exe System realtime
echo   ..\bin\test_realtime_console.exe Application recent
echo   ..\bin\test_realtime_console.exe Security all
echo.
echo Run '..\bin\test_realtime_console.exe --help' for full usage
echo.
echo ========================================
