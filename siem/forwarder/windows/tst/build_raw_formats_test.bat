@echo off
REM Build script for raw event formats test

echo Building raw event formats test...

g++ -I../inc ^
    test_raw_event_formats.cpp ^
    ../src/event_log_reader.cpp ^
    ../src/json_utils.cpp ^
    ../src/logger.cpp ^
    -o ../bin/test_raw_formats.exe ^
    -lwevtapi ^
    -std=c++17 ^
    -Wall

if %ERRORLEVEL% EQU 0 (
    echo Build successful! Executable: bin\test_raw_formats.exe
    echo.
    echo To run the test:
    echo   bin\test_raw_formats.exe
) else (
    echo Build failed!
    exit /b 1
)
