@echo off
REM Simple installation script without virtual environment
REM Use this if setup.bat has issues

echo ========================================
echo SIEM - Simple Installation
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python is not installed or not in PATH
    echo Please install Python 3.7+ from https://www.python.org/
    pause
    exit /b 1
)

echo [INFO] Python found:
python --version
echo.

REM Install dependencies directly (no virtual environment)
echo [INFO] Installing Flask and dependencies...
echo.

python -m pip install --upgrade pip
python -m pip install flask==3.0.0 werkzeug==3.0.1

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo Installation Complete!
    echo ========================================
    echo.
    echo To run the application:
    echo   python src\app.py
    echo.
    echo Access the web interface at:
    echo   http://localhost:5000
    echo.
    echo Log receiver listens on:
    echo   TCP port 8089
    echo ========================================
) else (
    echo.
    echo [ERROR] Installation failed
    echo.
    echo Try running as Administrator:
    echo   Right-click this file and select "Run as administrator"
    echo.
    echo Or install manually:
    echo   pip install flask==3.0.0 werkzeug==3.0.1
    pause
    exit /b 1
)

pause
