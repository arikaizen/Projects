@echo off
REM ========================================
REM Build script for Event Log Reader Test
REM ========================================

echo ========================================
echo Event Log Reader - Standalone Test
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
echo [2/3] Compiling test_event_log_reader_standalone.cpp...
g++ -I../inc ^
    test_event_log_reader_standalone.cpp ^
    ../src/event_log_reader.cpp ^
    ../src/json_utils.cpp ^
    ../src/logger.cpp ^
    -o ../bin/test_event_log_reader.exe ^
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
echo Executable created: ..\bin\test_event_log_reader.exe
echo.
echo To run the test:
echo   ..\bin\test_event_log_reader.exe
echo.
echo ========================================
