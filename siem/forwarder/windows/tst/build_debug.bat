@echo off
REM ========================================
REM Build Debug Diagnostic Tool
REM ========================================

echo ========================================
echo Event Log Reader - Debug Diagnostic
echo ========================================
echo.

REM Check if MinGW-w64 is in PATH
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] g++ not found in PATH
    exit /b 1
)

echo [1/2] Compiling debug tool...
g++ -I..\inc ^
    test_event_log_reader_debug.cpp ^
    ..\src\event_log_reader.cpp ^
    ..\src\json_utils.cpp ^
    ..\src\logger.cpp ^
    -o ..\bin\test_debug.exe ^
    -lwevtapi ^
    -std=c++17

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Compilation failed
    exit /b 1
)

echo.
echo [2/2] Build complete!
echo ========================================
echo.
echo Running diagnostic tool...
echo.

..\bin\test_debug.exe

echo.
echo ========================================
