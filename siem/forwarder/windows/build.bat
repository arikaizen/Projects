@echo off
REM ============================================================================
REM Windows Event Log Forwarder - Automatic Build Script
REM ============================================================================
REM
REM This script automates the complete build process for the Windows Event Log
REM Forwarder. It performs the following steps:
REM   1. Validates that CMake is installed and accessible
REM   2. Detects available C++ compilers (Visual Studio or MinGW-w64)
REM   3. Generates build files using CMake with the appropriate generator
REM   4. Compiles the project into executable binaries
REM   5. Places output files in the bin/ directory
REM
REM Requirements:
REM   - CMake 3.10 or higher
REM   - Visual Studio 2019+ OR MinGW-w64 with Windows SDK headers
REM   - Windows SDK (included with VS, required separately for MinGW)
REM
REM Usage:
REM   Simply run: build.bat
REM
REM Output:
REM   - bin/log_forwarder.exe  (Main forwarder application)
REM   - bin/test_forwarder.exe (Test/mock SIEM server)
REM ============================================================================

echo ====================================
echo Windows Event Log Forwarder Builder
echo ====================================
echo.

REM ============================================================================
REM STEP 1: Verify CMake Installation
REM ============================================================================
REM CMake is required to generate the build files for this project.
REM The 'where' command searches for cmake.exe in the system PATH.
REM If not found (%ERRORLEVEL% NEQ 0), display error and exit.

echo [1/4] Checking for CMake...
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: CMake is not installed or not in PATH
    echo ============================================================
    echo.
    echo CMake is required to build this project.
    echo.
    echo TROUBLESHOOTING:
    echo   1. Check if CMake is installed: where cmake
    echo   2. If installed, add CMake to your PATH environment variable
    echo   3. If not installed, download from: https://cmake.org/download/
    echo.
    echo QUICK FIX:
    echo   Run 'install_prerequisites.ps1' for automatic installation
    echo.
    pause
    exit /b 1
)
echo       [OK] CMake found
echo.

REM ============================================================================
REM STEP 2: Detect C++ Compiler
REM ============================================================================
REM The script checks for compilers in this priority order:
REM   1. MinGW-w64 (RECOMMENDED - ~200MB, includes Windows SDK)
REM   2. Visual Studio 2022 (alternative - ~7GB, comprehensive)
REM   3. Visual Studio 2019 (alternative - ~7GB, comprehensive)
REM
REM Why this order?
REM   - MinGW-w64 is lightweight and perfect for Windows 10
REM   - Includes winevt.h and all necessary Windows Event Log headers
REM   - Visual Studio is heavier but works if already installed

echo [2/4] Detecting C++ compiler...
SET COMPILER_TYPE=unknown
SET CMAKE_GENERATOR=

REM Check for MinGW-w64 FIRST (RECOMMENDED - lightweight, complete SDK)
REM The 'where' command checks if g++.exe is in the system PATH
REM MinGW-w64 includes winevt.h and all necessary Windows headers
where g++ >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo       [OK] Detected MinGW g++ compiler
    echo       Recommended: MinGW-w64 (lightweight, includes Windows SDK)
    SET COMPILER_TYPE=MinGW
    SET CMAKE_GENERATOR=-G "MinGW Makefiles"
    goto :found_compiler
)

REM Check for Visual Studio 2022 (alternative - heavier option)
REM Location: C:\Program Files\Microsoft Visual Studio\2022\
if exist "C:\Program Files\Microsoft Visual Studio\2022" (
    echo       [OK] Detected Visual Studio 2022 (alternative to MinGW-w64)
    SET COMPILER_TYPE=VS2022
    SET CMAKE_GENERATOR=-G "Visual Studio 17 2022" -A x64
    goto :found_compiler
)

REM Check for Visual Studio 2019 (alternative - heavier option)
REM Location: C:\Program Files (x86)\Microsoft Visual Studio\2019\
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019" (
    echo       [OK] Detected Visual Studio 2019 (alternative to MinGW-w64)
    SET COMPILER_TYPE=VS2019
    SET CMAKE_GENERATOR=-G "Visual Studio 16 2019" -A x64
    goto :found_compiler
)

REM ============================================================================
REM ERROR: No Compiler Found
REM ============================================================================
REM If we reach this point, no suitable compiler was detected.
REM Display detailed troubleshooting information and exit.

echo.
echo ============================================================
echo ERROR: No C++ compiler found!
echo ============================================================
echo.
echo This project requires a C++ compiler with Windows SDK support.
echo.
echo RECOMMENDED SOLUTIONS:
echo.
echo Option 1 - Install MinGW-w64 (Recommended - ~200MB):
echo   1. Download from: https://github.com/niXman/mingw-builds-binaries/releases
echo   2. Get file: x86_64-*-release-posix-seh-ucrt-*.7z
echo   3. Extract to C:\mingw64
echo   4. Add C:\mingw64\bin to your PATH environment variable
echo   5. Restart terminal and run build.bat again
echo.
echo Option 2 - Install Visual Studio Community (Free - ~7GB):
echo   1. Download from: https://visualstudio.microsoft.com/downloads/
echo   2. Select "Community" edition (free for individuals)
echo   3. Install "Desktop development with C++" workload
echo   4. Restart terminal and run build.bat again
echo.
echo AUTOMATED SETUP:
echo   Run 'install_prerequisites.ps1' in PowerShell for guided setup
echo.
pause
exit /b 1

:found_compiler
echo       Compiler: %COMPILER_TYPE%
echo.

REM ============================================================================
REM STEP 3: Generate Build Files with CMake
REM ============================================================================
REM Clean any previous build to avoid conflicts between generators
REM (e.g., switching from MinGW Makefiles to Visual Studio or vice versa)
REM Then create fresh build directory and run CMake to generate build files.

echo [3/4] Generating build files...

REM Remove old build directory if it exists
REM This prevents generator conflicts and ensures a clean build
if exist build (
    echo       Cleaning old build directory...
    rmdir /s /q build
)

REM Create new build directory and enter it
REM CMake uses out-of-source builds to keep source tree clean
mkdir build
cd build

REM Run CMake to generate build files
REM %CMAKE_GENERATOR% contains the generator flag (-G "...") set above
REM The ".." points to parent directory containing CMakeLists.txt
echo       Running CMake with %COMPILER_TYPE%...
cmake .. %CMAKE_GENERATOR%

REM Check if CMake succeeded
REM %ERRORLEVEL% contains the exit code of the last command
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: CMake configuration failed
    echo ============================================================
    echo.
    echo CMake was unable to generate build files.
    echo.
    echo COMMON CAUSES:
    echo   - Missing Windows SDK (required for winevt.h header)
    echo   - Incorrect compiler installation
    echo   - CMakeLists.txt syntax errors
    echo   - Missing required libraries (ws2_32.lib, wevtapi.lib)
    echo.
    echo SOLUTION FOR MINGW USERS:
    echo   If using MinGW, ensure you have MinGW-w64 (not standard MinGW)
    echo   Standard MinGW lacks Windows Event Log API headers
    echo.
    echo Check the error messages above for specific details.
    echo.
    cd ..
    pause
    exit /b 1
)
echo       [OK] Build files generated successfully
echo.

REM ============================================================================
REM STEP 4: Compile the Project
REM ============================================================================
REM Use the appropriate build tool based on the compiler:
REM   - MinGW: Use mingw32-make (GNU Make for Windows)
REM   - Visual Studio: Use cmake --build (invokes MSBuild)

echo [4/4] Compiling project...

REM Choose build command based on compiler type
if "%COMPILER_TYPE%"=="MinGW" (
    REM For MinGW, use mingw32-make with -j4 for parallel compilation (4 jobs)
    REM This speeds up compilation on multi-core processors
    echo       Building with MinGW Make (4 parallel jobs)...
    mingw32-make -j4
) else (
    REM For Visual Studio, use cmake --build in Release configuration
    REM Release mode enables optimizations and creates smaller executables
    echo       Building with Visual Studio (Release mode)...
    cmake --build . --config Release
)

REM Check if compilation succeeded
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ============================================================
    echo ERROR: Build failed
    echo ============================================================
    echo.
    echo The compilation process encountered errors.
    echo.
    echo COMMON ISSUES:
    echo   - Missing Windows SDK headers (winevt.h, winsock2.h)
    echo   - Syntax errors in source code
    echo   - Missing libraries (ws2_32.lib, wevtapi.lib)
    echo   - Incompatible compiler version
    echo.
    echo FOR MINGW USERS - CRITICAL:
    echo   If you see "winevt.h: No such file or directory"
    echo   You need MinGW-w64, NOT standard MinGW
    echo   Download: https://github.com/niXman/mingw-builds-binaries/releases
    echo.
    echo Check the error messages above for specific details.
    echo.
    cd ..
    pause
    exit /b 1
)

REM Return to project root directory
cd ..

REM ============================================================================
REM Verify Output Files Were Created
REM ============================================================================
REM Check that both executables were successfully built
REM Sometimes make succeeds partially but doesn't create all targets

echo.
echo Verifying build outputs...

if not exist "bin\log_forwarder.exe" (
    echo.
    echo ============================================================
    echo ERROR: log_forwarder.exe was not created
    echo ============================================================
    echo.
    echo The build process completed but log_forwarder.exe is missing.
    echo.
    echo MANUAL BUILD WORKAROUND:
    echo   You can compile manually with this command:
    echo.
    echo   cd siem\forwarder\windows
    echo   g++ -I./inc src/main.cpp src/log_forwarder.cpp src/event_log_reader.cpp src/json_utils.cpp src/forwarder_api.cpp -o bin/log_forwarder.exe -lwevtapi -lws2_32 -std=c++17
    echo.
    pause
    exit /b 1
)

if not exist "bin\test_forwarder.exe" (
    echo [WARNING] test_forwarder.exe was not created
)

REM ============================================================================
REM SUCCESS: Build Complete
REM ============================================================================
REM Display success message and output file locations

echo.
echo ============================================================
echo BUILD SUCCESSFUL!
echo ============================================================
echo.
echo Compiled with: %COMPILER_TYPE%
echo.
echo Output files:
echo   [Forwarder] bin\log_forwarder.exe  - Main application
echo   [Test]      bin\test_forwarder.exe - Mock SIEM server
echo.
echo NEXT STEPS:
echo.
echo 1. Test the forwarder:
echo    Terminal 1: bin\test_forwarder.exe
echo    Terminal 2: bin\log_forwarder.exe
echo.
echo 2. Connect to real SIEM server:
echo    bin\log_forwarder.exe [server_ip] [port]
echo    Example: bin\log_forwarder.exe 192.168.1.100 8089
echo.
echo 3. Run as Administrator (required for Security event logs):
echo    Right-click bin\log_forwarder.exe -^> Run as administrator
echo.
echo ============================================================
echo.

pause
