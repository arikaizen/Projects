@echo off
REM ============================================================================
REM Windows Event Log Forwarder - Test Build Script
REM ============================================================================
REM
REM This script compiles and runs unit tests using Google Test framework
REM
REM Requirements:
REM   - MinGW-w64 with g++ compiler
REM   - Google Test library (gtest)
REM
REM Usage:
REM   build_tests.bat [test_name]
REM
REM   If test_name is provided, only that test will run
REM   Otherwise, all tests will run
REM
REM Output:
REM   - test/bin/test_runner.exe  (Test executable)
REM ============================================================================

echo ====================================
echo Windows Event Log Forwarder - Tests
echo ====================================
echo.

REM ============================================================================
REM STEP 1: Verify g++ Compiler
REM ============================================================================
echo [1/5] Checking for g++ compiler...
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: g++ compiler not found!
    echo ============================================================
    echo.
    echo Please install MinGW-w64 and add it to PATH
    echo.
    pause
    exit /b 1
)
echo       [OK] g++ compiler found
echo.

REM ============================================================================
REM STEP 2: Check for Google Test
REM ============================================================================
echo [2/5] Checking for Google Test library...
if not exist "googletest" (
    echo.
    echo ============================================================
    echo ERROR: Google Test not found!
    echo ============================================================
    echo.
    echo Please download and extract Google Test to the test directory.
    echo.
    echo Instructions:
    echo   1. Download: https://github.com/google/googletest/releases
    echo   2. Extract to: test/googletest/
    echo   3. Run this script again
    echo.
    echo Or run: git clone https://github.com/google/googletest.git
    echo.
    pause
    exit /b 1
)
echo       [OK] Google Test found
echo.

REM ============================================================================
REM STEP 3: Build Google Test Library
REM ============================================================================
echo [3/5] Building Google Test library...
if not exist "googletest\build" mkdir googletest\build
cd googletest\build

if not exist "libgtest.a" (
    echo       Compiling Google Test...
    g++ -std=c++17 -isystem ../googletest/include -I../googletest -pthread -c ../googletest/src/gtest-all.cc -o gtest-all.o
    ar -rv libgtest.a gtest-all.o
    g++ -std=c++17 -isystem ../googletest/include -I../googletest -pthread -c ../googletest/src/gtest_main.cc -o gtest_main.o
    ar -rv libgtest_main.a gtest_main.o
    echo       [OK] Google Test compiled
) else (
    echo       [OK] Google Test already compiled
)

cd ..\..
echo.

REM ============================================================================
REM STEP 4: Create bin Directory
REM ============================================================================
echo [4/5] Preparing output directory...
if not exist "bin" mkdir bin
echo       [OK] bin directory ready
echo.

REM ============================================================================
REM STEP 5: Compile and Link Tests
REM ============================================================================
echo [5/5] Compiling tests...
echo.

REM Compile each test file
echo       Compiling test_json_utils.cpp...
g++ -std=c++17 -I../inc -isystem googletest/googletest/include -c test_json_utils.cpp -o bin/test_json_utils.o

echo       Compiling test_logger.cpp...
g++ -std=c++17 -I../inc -isystem googletest/googletest/include -c test_logger.cpp -o bin/test_logger.o

echo       Compiling test_log_forwarder.cpp...
g++ -std=c++17 -I../inc -isystem googletest/googletest/include -c test_log_forwarder.cpp -o bin/test_log_forwarder.o

echo       Compiling test_event_log_reader.cpp...
g++ -std=c++17 -I../inc -isystem googletest/googletest/include -c test_event_log_reader.cpp -o bin/test_event_log_reader.o

echo       Compiling test_main.cpp...
g++ -std=c++17 -isystem googletest/googletest/include -c test_main.cpp -o bin/test_main.o

REM Compile source files
echo       Compiling source files...
g++ -std=c++17 -I../inc -c ../src/json_utils.cpp -o bin/json_utils.o
g++ -std=c++17 -I../inc -c ../src/logger.cpp -o bin/logger.o
g++ -std=c++17 -I../inc -c ../src/log_forwarder.cpp -o bin/log_forwarder.o
g++ -std=c++17 -I../inc -c ../src/event_log_reader.cpp -o bin/event_log_reader.o

REM Link everything together
echo       Linking test executable...
g++ -std=c++17 ^
    bin/test_main.o ^
    bin/test_json_utils.o ^
    bin/test_logger.o ^
    bin/test_log_forwarder.o ^
    bin/test_event_log_reader.o ^
    bin/json_utils.o ^
    bin/logger.o ^
    bin/log_forwarder.o ^
    bin/event_log_reader.o ^
    -Lgoogletest/build ^
    -lgtest ^
    -lwevtapi ^
    -lws2_32 ^
    -pthread ^
    -o bin/test_runner.exe

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: Build failed
    echo ============================================================
    echo.
    pause
    exit /b 1
)

echo.
echo ============================================================
echo BUILD SUCCESSFUL!
echo ============================================================
echo.

REM ============================================================================
REM Run Tests
REM ============================================================================
echo.
echo ============================================================
echo Running Tests...
echo ============================================================
echo.

if "%1"=="" (
    REM Run all tests
    bin\test_runner.exe
) else (
    REM Run specific test
    bin\test_runner.exe --gtest_filter=%1
)

set TEST_RESULT=%ERRORLEVEL%

echo.
echo ============================================================
if %TEST_RESULT% EQU 0 (
    echo All tests PASSED!
) else (
    echo Some tests FAILED!
)
echo ============================================================
echo.

pause
exit /b %TEST_RESULT%
