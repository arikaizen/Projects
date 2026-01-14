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

REM Activate virtual environment
echo [INFO] Activating virtual environment...
call venv\Scripts\activate.bat

REM Check if Flask is installed
python -c "import flask" 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Flask not installed in virtual environment
    echo [INFO] Run setup.bat to install dependencies
    exit /b 1
)

REM Run the application
echo [INFO] Starting application...
echo.
python src\app.py
