#!/bin/bash
# Dependency checker and installer for Asset Map application
# This script checks if required Python packages are installed and installs them if missing

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
REQUIREMENTS_FILE="$PROJECT_ROOT/src/py/requirements.txt"

echo "========================================="
echo "Asset Map Dependency Checker"
echo "========================================="
echo ""

# Check if Python is installed
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: Python3 is not installed${NC}"
    echo "Please install Python 3.7+ from https://www.python.org/downloads/"
    exit 1
fi

PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}')
echo -e "${GREEN}✓${NC} Python found: $PYTHON_VERSION"

# Check if pip is installed
if ! command -v pip3 &> /dev/null && ! python3 -m pip --version &> /dev/null; then
    echo -e "${RED}Error: pip is not installed${NC}"
    echo "Installing pip..."
    python3 -m ensurepip --upgrade
fi

echo -e "${GREEN}✓${NC} pip is available"

# Check if requirements.txt exists
if [ ! -f "$REQUIREMENTS_FILE" ]; then
    echo -e "${RED}Error: requirements.txt not found at $REQUIREMENTS_FILE${NC}"
    exit 1
fi

echo -e "${GREEN}✓${NC} Found requirements.txt"
echo ""

# Function to check if a package is installed
check_package() {
    local package=$1
    python3 -c "import importlib.util; import sys; sys.exit(0 if importlib.util.find_spec('${package}') else 1)" 2>/dev/null
}

# Read requirements and check each package
echo "Checking dependencies..."
MISSING_PACKAGES=()

while IFS= read -r line || [ -n "$line" ]; do
    # Skip empty lines and comments
    [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]] && continue

    # Extract package name (before ==, >=, etc.)
    PACKAGE=$(echo "$line" | sed 's/[>=<].*//' | sed 's/\[.*//' | xargs)

    # Skip if empty after processing
    [ -z "$PACKAGE" ] && continue

    # Convert package name to import name (handle common cases)
    IMPORT_NAME=$(echo "$PACKAGE" | tr '[:upper:]' '[:lower:]' | tr '-' '_')

    # Special cases for package vs import names
    case "$IMPORT_NAME" in
        "fastapi") IMPORT_NAME="fastapi" ;;
        "uvicorn") IMPORT_NAME="uvicorn" ;;
        "jinja2") IMPORT_NAME="jinja2" ;;
    esac

    if check_package "$IMPORT_NAME"; then
        echo -e "  ${GREEN}✓${NC} $PACKAGE"
    else
        echo -e "  ${RED}✗${NC} $PACKAGE (missing)"
        MISSING_PACKAGES+=("$line")
    fi
done < "$REQUIREMENTS_FILE"

echo ""

# Install missing packages if any
if [ ${#MISSING_PACKAGES[@]} -eq 0 ]; then
    echo -e "${GREEN}All dependencies are already installed!${NC}"
    exit 0
else
    echo -e "${YELLOW}Found ${#MISSING_PACKAGES[@]} missing package(s)${NC}"
    echo ""

    # Ask for confirmation
    read -p "Do you want to install missing dependencies? (y/n) " -n 1 -r
    echo ""

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Installing missing dependencies..."

        # Try to install all requirements at once
        if python3 -m pip install -r "$REQUIREMENTS_FILE" --quiet; then
            echo -e "${GREEN}✓ All dependencies installed successfully!${NC}"
        else
            echo -e "${RED}Error: Failed to install some dependencies${NC}"
            echo "Try running manually: pip3 install -r $REQUIREMENTS_FILE"
            exit 1
        fi
    else
        echo -e "${YELLOW}Installation skipped${NC}"
        echo "To install manually, run:"
        echo "  pip3 install -r $REQUIREMENTS_FILE"
        exit 1
    fi
fi

echo ""
echo "========================================="
echo "Setup complete! You can now run:"
echo "  cd $PROJECT_ROOT/src/py"
echo "  python3 app.py"
echo "========================================="
