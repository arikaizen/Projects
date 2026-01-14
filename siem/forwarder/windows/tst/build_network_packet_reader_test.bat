@echo off
REM ============================================================================
REM Build Network Packet Reader Tests (No Google Test)
REM ============================================================================

echo ====================================
echo Network Packet Reader Tests
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
    test_network_packet_reader.cpp ^
    ../src/network_packet_reader.cpp ^
    ../src/json_utils.cpp ^
    ../src/logger.cpp ^
    -o bin/test_network_packet_reader.exe ^
    -lws2_32 ^
    -std=c++17 ^
    -Wall

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo.
echo ========================================
echo BUILD SUCCESSFUL!
echo ========================================
echo.
echo Test executable created: bin\test_network_packet_reader.exe
echo.
echo To run the tests:
echo   bin\test_network_packet_reader.exe
echo.

exit /b 0
