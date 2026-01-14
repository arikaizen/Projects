@echo off
REM Setup script for SIEM Main Application
REM This script creates a virtual environment and installs dependencies

echo ========================================
echo SIEM Main Application Setup
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python is not installed or not in PATH
    echo Please install Python 3.7+ from https://www.python.org/
    exit /b 1
)

echo [INFO] Python found:
python --version
echo.

REM Check if venv exists
if exist venv (
    echo [INFO] Virtual environment already exists
    echo.
) else (
    echo [INFO] Creating virtual environment...
    python -m venv venv
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Failed to create virtual environment
        exit /b 1
    )
    echo [SUCCESS] Virtual environment created
    echo.
)

REM Activate virtual environment
echo [INFO] Activating virtual environment...
call venv\Scripts\activate.bat
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to activate virtual environment
    exit /b 1
)
echo.

REM Upgrade pip
echo [INFO] Upgrading pip...
python -m pip install --upgrade pip
echo.

REM Install dependencies
echo [INFO] Installing dependencies from requirements.txt...
pip install -r requirements.txt
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to install dependencies
    exit /b 1
)
echo.

echo ========================================
echo Setup Complete!
echo ========================================
echo.
echo To run the application:
echo   1. Activate the virtual environment:
echo      venv\Scripts\activate
echo.
echo   2. Run the application:
echo      python src\app.py
echo.
echo   3. Access the web interface:
echo      http://localhost:5000
echo.
echo   4. Log receiver listens on:
echo      TCP port 8089
echo ========================================
