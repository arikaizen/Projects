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

REM Install using venv's python directly (no need to activate)
echo [INFO] Installing packages in virtual environment...
echo.

REM Upgrade pip
echo [INFO] Upgrading pip...
venv\Scripts\python.exe -m pip install --upgrade pip
if %ERRORLEVEL% NEQ 0 (
    echo [WARNING] Failed to upgrade pip, continuing anyway...
)
echo.

REM Install dependencies
echo [INFO] Installing dependencies from requirements.txt...
venv\Scripts\python.exe -m pip install -r requirements.txt
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Failed to install dependencies
    echo [INFO] Trying alternative method...
    python -m pip install -r requirements.txt
    if %ERRORLEVEL% NEQ 0 (
        echo [ERROR] Still failed. Please check your Python installation.
        exit /b 1
    )
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
