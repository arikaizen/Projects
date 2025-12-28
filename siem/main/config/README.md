# SIEM Configuration

This directory contains configuration and setup scripts for the SIEM application.

## Dependency Checker

The `check_dependencies.sh` (Linux/Mac) and `check_dependencies.bat` (Windows) scripts automatically check if all required Python packages are installed and offer to install them if missing.

### Usage

**On Linux/Mac:**
```bash
cd siem/main/config
./check_dependencies.sh
```

**On Windows:**
```cmd
cd siem\main\config
check_dependencies.bat
```

### What It Does

1. ✓ Checks if Python 3.7+ is installed
2. ✓ Verifies pip is available
3. ✓ Reads requirements.txt
4. ✓ Checks each package (flask, werkzeug)
5. ✓ Offers to install missing packages
6. ✓ Provides instructions to run the app

### Manual Installation

If you prefer to install dependencies manually:

```bash
cd siem/main
pip install -r requirements.txt
```

Or with a virtual environment:
```bash
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
pip install -r requirements.txt
```
