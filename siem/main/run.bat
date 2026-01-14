@echo off
REM Quick run script for SIEM Main Application

echo ========================================
echo Starting SIEM Main Application
echo ========================================
echo.

REM Check if virtual environment exists
if not exist venv (
    echo [WARNING] Virtual environment not found
    echo [INFO] Run setup.bat first to create it
    echo.
    echo Attempting to run with system Python...
    python src\app.py
    exit /b
)

REM Check if Flask is installed in venv
echo [INFO] Checking Flask installation...
venv\Scripts\python.exe -c "import flask" 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Flask not installed in virtual environment
    echo [INFO] Run setup.bat to install dependencies
    pause
    exit /b 1
)

REM Run the application using venv's python
echo [INFO] Starting application with virtual environment...
echo.
venv\Scripts\python.exe src\app.py
