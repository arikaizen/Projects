@echo off
REM Dependency checker and installer for SIEM application (Windows)
REM This script checks if required Python packages are installed and installs them if missing

setlocal EnableDelayedExpansion

echo =========================================
echo SIEM Dependency Checker
echo =========================================
echo.

REM Get script directory and project root
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "REQUIREMENTS_FILE=%PROJECT_ROOT%\requirements.txt"

REM Check if Python is installed
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python is not installed or not in PATH
    echo Please install Python 3.7+ from https://www.python.org/downloads/
    echo Make sure to check "Add Python to PATH" during installation
    pause
    exit /b 1
)

for /f "tokens=2" %%i in ('python --version 2^>^&1') do set PYTHON_VERSION=%%i
echo [OK] Python found: %PYTHON_VERSION%

REM Check if pip is available
python -m pip --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] pip is not available
    echo Installing pip...
    python -m ensurepip --upgrade
)

echo [OK] pip is available

REM Check if requirements.txt exists
if not exist "%REQUIREMENTS_FILE%" (
    echo [ERROR] requirements.txt not found at %REQUIREMENTS_FILE%
    pause
    exit /b 1
)

echo [OK] Found requirements.txt
echo.

echo Checking dependencies...

REM Check each package in requirements.txt
set MISSING_COUNT=0
for /f "usebackq tokens=* delims=" %%a in ("%REQUIREMENTS_FILE%") do (
    set "LINE=%%a"
    REM Skip empty lines and comments
    if not "!LINE!"=="" (
        echo !LINE! | findstr /r "^[^#]" >nul
        if !errorlevel! equ 0 (
            REM Extract package name
            for /f "tokens=1 delims=>=<[" %%p in ("!LINE!") do (
                set "PACKAGE=%%p"
                REM Remove spaces
                set "PACKAGE=!PACKAGE: =!"

                if not "!PACKAGE!"=="" (
                    REM Convert to import name (lowercase, - to _)
                    set "IMPORT_NAME=!PACKAGE:-=_!"
                    call :lowercase IMPORT_NAME

                    REM Check if package is installed
                    python -c "import !IMPORT_NAME!" >nul 2>&1
                    if !errorlevel! equ 0 (
                        echo   [OK] !PACKAGE!
                    ) else (
                        echo   [MISSING] !PACKAGE!
                        set /a MISSING_COUNT+=1
                    )
                )
            )
        )
    )
)

echo.

if %MISSING_COUNT% equ 0 (
    echo [SUCCESS] All dependencies are already installed!
    echo.
    goto :end_success
)

echo [WARNING] Found %MISSING_COUNT% missing package(s)
echo.

set /p "INSTALL=Do you want to install missing dependencies? (Y/N): "
if /i not "%INSTALL%"=="Y" (
    echo Installation skipped
    echo To install manually, run:
    echo   pip install -r "%REQUIREMENTS_FILE%"
    pause
    exit /b 1
)

echo.
echo Installing missing dependencies...
python -m pip install -r "%REQUIREMENTS_FILE%" --quiet

if %errorlevel% equ 0 (
    echo [SUCCESS] All dependencies installed successfully!
) else (
    echo [ERROR] Failed to install some dependencies
    echo Try running manually: pip install -r "%REQUIREMENTS_FILE%"
    pause
    exit /b 1
)

:end_success
echo.
echo =========================================
echo Setup complete! You can now run:
echo   cd %PROJECT_ROOT%\src
echo   python app.py
echo =========================================
echo.
pause
exit /b 0

:lowercase
REM Function to convert string to lowercase
set "%~1=!%~1:A=a!"
set "%~1=!%~1:B=b!"
set "%~1=!%~1:C=c!"
set "%~1=!%~1:D=d!"
set "%~1=!%~1:E=e!"
set "%~1=!%~1:F=f!"
set "%~1=!%~1:G=g!"
set "%~1=!%~1:H=h!"
set "%~1=!%~1:I=i!"
set "%~1=!%~1:J=j!"
set "%~1=!%~1:K=k!"
set "%~1=!%~1:L=l!"
set "%~1=!%~1:M=m!"
set "%~1=!%~1:N=n!"
set "%~1=!%~1:O=o!"
set "%~1=!%~1:P=p!"
set "%~1=!%~1:Q=q!"
set "%~1=!%~1:R=r!"
set "%~1=!%~1:S=s!"
set "%~1=!%~1:T=t!"
set "%~1=!%~1:U=u!"
set "%~1=!%~1:V=v!"
set "%~1=!%~1:W=w!"
set "%~1=!%~1:X=x!"
set "%~1=!%~1:Y=y!"
set "%~1=!%~1:Z=z!"
goto :eof
