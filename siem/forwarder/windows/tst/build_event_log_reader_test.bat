@echo off
REM ============================================================================
REM Build and Run Event Log Reader Tests (No Google Test)
REM ============================================================================

echo ====================================
echo Event Log Reader Tests
echo ====================================
echo.

echo [1/2] Checking for g++ compiler...
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] g++ not found in PATH
    exit /b 1
)
echo       [OK] g++ compiler found
echo.

if not exist "bin" mkdir bin

echo [2/2] Compiling tests...
echo.

g++ -I../inc ^
    test_event_log_reader_simple.cpp ^
    ../src/event_log_reader.cpp ^
    ../src/json_utils.cpp ^
    ../src/logger.cpp ^
    -o bin/test_event_log_reader.exe ^
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
bin\test_event_log_reader.exe

exit /b %ERRORLEVEL%
