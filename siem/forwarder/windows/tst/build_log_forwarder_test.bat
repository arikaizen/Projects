@echo off
REM Build script for log_forwarder tests
REM Compiles test_log_forwarder.cpp with necessary dependencies

echo ========================================
echo Building Log Forwarder Tests
echo ========================================
echo.

REM Set paths
set TEST_FILE=test_log_forwarder.cpp
set OUTPUT_FILE=..\bin\test_log_forwarder.exe
set INC_DIR=..\inc
set SRC_DIR=..\src

REM Check if Visual Studio environment is set up
where cl.exe >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Visual Studio compiler not found!
    echo Please run this from a Visual Studio Developer Command Prompt
    echo Or run vcvarsall.bat first
    exit /b 1
)

echo [INFO] Compiling test_log_forwarder.cpp...
echo.

REM Compile the test with dependencies
REM Include log_forwarder.cpp and logger.cpp for dependencies
cl.exe /EHsc /std:c++17 /I"%INC_DIR%" ^
    %TEST_FILE% ^
    %SRC_DIR%\log_forwarder.cpp ^
    %SRC_DIR%\logger.cpp ^
    /Fe:"%OUTPUT_FILE%" ^
    /link ws2_32.lib

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Build Successful!
    echo ========================================
    echo Output: %OUTPUT_FILE%
    echo.
    echo To run the test:
    echo   cd ..\bin
    echo   test_log_forwarder.exe
    echo.
) else (
    echo.
    echo ========================================
    echo Build Failed!
    echo ========================================
    exit /b 1
)
