@echo off
REM ============================================================================
REM Simple Test Build (NO Google Test required)
REM ============================================================================

echo ====================================
echo Event Log Reader Simple Tests
echo ====================================
echo.

echo Compiling test program...

if not exist "bin" mkdir bin

g++ -I../inc ^
    test_event_log_reader_simple.cpp ^
    ../src/event_log_reader.cpp ^
    ../src/json_utils.cpp ^
    ../src/logger.cpp ^
    -o bin/test_simple.exe ^
    -lwevtapi ^
    -std=c++17 ^
    -Wall

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo [OK] Build successful!
echo.

echo Running tests...
echo.
bin\test_simple.exe

exit /b %ERRORLEVEL%
