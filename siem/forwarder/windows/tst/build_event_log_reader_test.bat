@echo off
REM ============================================================================
REM Build and Run Event Log Reader Tests
REM ============================================================================
REM
REM This script builds and runs only the event_log_reader tests
REM
REM Usage:
REM   build_event_log_reader_test.bat [test_name]
REM
REM   If test_name is provided, only that specific test will run
REM   Otherwise, all event_log_reader tests will run
REM
REM Example:
REM   build_event_log_reader_test.bat
REM   build_event_log_reader_test.bat EventLogReaderTest.GetRawEventXml_ReturnsXml
REM ============================================================================

echo ====================================
echo Event Log Reader Tests
echo ====================================
echo.

REM Check for g++ compiler
echo [1/4] Checking for g++ compiler...
where g++ >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] g++ not found in PATH
    exit /b 1
)
echo       [OK] g++ compiler found
echo.

REM Check for Google Test
echo [2/4] Checking for Google Test library...
if not exist "googletest" (
    echo [ERROR] Google Test not found in tst/googletest/
    echo Please run build_tests.bat first to set up Google Test
    exit /b 1
)
echo       [OK] Google Test found
echo.

REM Build Google Test if needed
echo [3/4] Building Google Test library...
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

REM Create bin directory
if not exist "bin" mkdir bin

REM Compile and link
echo [4/4] Compiling event_log_reader tests...
echo.

echo       Compiling test_event_log_reader.cpp...
g++ -std=c++17 -I../inc -isystem googletest/googletest/include -c test_event_log_reader.cpp -o bin/test_event_log_reader.o

echo       Compiling source files...
g++ -std=c++17 -I../inc -c ../src/event_log_reader.cpp -o bin/event_log_reader.o
g++ -std=c++17 -I../inc -c ../src/json_utils.cpp -o bin/json_utils.o
g++ -std=c++17 -I../inc -c ../src/logger.cpp -o bin/logger.o

echo       Compiling test_main.cpp...
g++ -std=c++17 -isystem googletest/googletest/include -c test_main.cpp -o bin/test_main.o

echo       Linking test executable...
g++ -std=c++17 ^
    bin/test_main.o ^
    bin/test_event_log_reader.o ^
    bin/event_log_reader.o ^
    bin/json_utils.o ^
    bin/logger.o ^
    -Lgoogletest/build ^
    -lgtest ^
    -lwevtapi ^
    -pthread ^
    -o bin/test_event_log_reader.exe

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] Build failed!
    exit /b 1
)

echo.
echo ============================================================
echo BUILD SUCCESSFUL!
echo ============================================================
echo.

REM Run tests
echo ============================================================
echo Running Event Log Reader Tests...
echo ============================================================
echo.

if "%1"=="" (
    REM Run all event_log_reader tests
    bin\test_event_log_reader.exe --gtest_filter=EventLogReaderTest.*
) else (
    REM Run specific test
    bin\test_event_log_reader.exe --gtest_filter=%1
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

exit /b %TEST_RESULT%
