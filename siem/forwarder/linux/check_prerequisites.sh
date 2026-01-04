#!/bin/bash
# Prerequisite Checker and Installer for Linux System Log Forwarder

echo "========================================"
echo "Linux System Log Forwarder"
echo "Prerequisite Checker"
echo "========================================"
echo ""

MISSING_TOOLS=0

# Detect package manager
if command -v apt &> /dev/null; then
    PKG_MANAGER="apt"
    INSTALL_CMD="sudo apt install -y"
elif command -v dnf &> /dev/null; then
    PKG_MANAGER="dnf"
    INSTALL_CMD="sudo dnf install -y"
elif command -v yum &> /dev/null; then
    PKG_MANAGER="yum"
    INSTALL_CMD="sudo yum install -y"
elif command -v pacman &> /dev/null; then
    PKG_MANAGER="pacman"
    INSTALL_CMD="sudo pacman -S --noconfirm"
else
    echo "[WARNING] Could not detect package manager"
    PKG_MANAGER="unknown"
fi

echo "Detected package manager: $PKG_MANAGER"
echo ""

# Check for CMake
echo "[1/4] Checking for CMake..."
if command -v cmake &> /dev/null; then
    VERSION=$(cmake --version | head -n1)
    echo "  [OK] $VERSION"
else
    echo "  [MISSING] CMake not found"
    MISSING_TOOLS=1
    MISSING_CMAKE=1
fi

# Check for C++ compiler
echo "[2/4] Checking for C++ compiler..."
if command -v g++ &> /dev/null; then
    VERSION=$(g++ --version | head -n1)
    echo "  [OK] $VERSION"
elif command -v clang++ &> /dev/null; then
    VERSION=$(clang++ --version | head -n1)
    echo "  [OK] $VERSION"
else
    echo "  [MISSING] C++ compiler not found"
    MISSING_TOOLS=1
    MISSING_COMPILER=1
fi

# Check for libsystemd
echo "[3/4] Checking for libsystemd..."
if pkg-config --exists libsystemd; then
    VERSION=$(pkg-config --modversion libsystemd)
    echo "  [OK] libsystemd found (version $VERSION)"
else
    echo "  [MISSING] libsystemd-dev not found"
    MISSING_TOOLS=1
    MISSING_SYSTEMD=1
fi

# Check for make
echo "[4/4] Checking for make..."
if command -v make &> /dev/null; then
    VERSION=$(make --version | head -n1)
    echo "  [OK] $VERSION"
else
    echo "  [MISSING] make not found"
    MISSING_TOOLS=1
    MISSING_MAKE=1
fi

echo ""
echo "========================================"
echo "Prerequisite Check Complete"
echo "========================================"
echo ""

if [ $MISSING_TOOLS -eq 0 ]; then
    echo "[SUCCESS] All prerequisites are installed!"
    echo ""
    echo "You can now build the forwarder by running:"
    echo "  ./build.sh"
    echo ""
    exit 0
fi

# Handle missing tools
echo "[WARNING] Some prerequisites are missing"
echo ""

if [ "$PKG_MANAGER" = "unknown" ]; then
    echo "Please install the following packages manually:"
    [ -n "$MISSING_CMAKE" ] && echo "  - cmake"
    [ -n "$MISSING_COMPILER" ] && echo "  - g++ or clang++"
    [ -n "$MISSING_SYSTEMD" ] && echo "  - libsystemd-dev"
    [ -n "$MISSING_MAKE" ] && echo "  - make"
    exit 1
fi

# Offer to install missing tools
echo "The following packages can be installed automatically:"
[ -n "$MISSING_CMAKE" ] && echo "  - cmake"
[ -n "$MISSING_COMPILER" ] && echo "  - build-essential (g++, make)"
[ -n "$MISSING_SYSTEMD" ] && echo "  - libsystemd-dev"
[ -n "$MISSING_MAKE" ] && echo "  - make"
echo ""

read -p "Would you like to install missing packages now? (y/n): " -n 1 -r
echo ""

if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "Installing prerequisites..."
    echo ""

    if [ "$PKG_MANAGER" = "apt" ]; then
        sudo apt update
        [ -n "$MISSING_CMAKE" ] && $INSTALL_CMD cmake
        [ -n "$MISSING_COMPILER" ] && $INSTALL_CMD build-essential
        [ -n "$MISSING_SYSTEMD" ] && $INSTALL_CMD libsystemd-dev
        [ -n "$MISSING_MAKE" ] && $INSTALL_CMD make
    elif [ "$PKG_MANAGER" = "dnf" ] || [ "$PKG_MANAGER" = "yum" ]; then
        [ -n "$MISSING_CMAKE" ] && $INSTALL_CMD cmake
        [ -n "$MISSING_COMPILER" ] && $INSTALL_CMD gcc-c++
        [ -n "$MISSING_SYSTEMD" ] && $INSTALL_CMD systemd-devel
        [ -n "$MISSING_MAKE" ] && $INSTALL_CMD make
    elif [ "$PKG_MANAGER" = "pacman" ]; then
        [ -n "$MISSING_CMAKE" ] && $INSTALL_CMD cmake
        [ -n "$MISSING_COMPILER" ] && $INSTALL_CMD base-devel
        [ -n "$MISSING_SYSTEMD" ] && $INSTALL_CMD systemd
        [ -n "$MISSING_MAKE" ] && $INSTALL_CMD make
    fi

    echo ""
    echo "========================================"
    echo "Installation Complete"
    echo "========================================"
    echo ""
    echo "Run this script again to verify: ./check_prerequisites.sh"
    echo "Then build the forwarder: ./build.sh"
    echo ""
else
    echo ""
    echo "========================================"
    echo "Manual Installation Required"
    echo "========================================"
    echo ""

    if [ "$PKG_MANAGER" = "apt" ]; then
        echo "Run the following commands:"
        echo "  sudo apt update"
        [ -n "$MISSING_CMAKE" ] && echo "  sudo apt install cmake"
        [ -n "$MISSING_COMPILER" ] && echo "  sudo apt install build-essential"
        [ -n "$MISSING_SYSTEMD" ] && echo "  sudo apt install libsystemd-dev"
    elif [ "$PKG_MANAGER" = "dnf" ]; then
        echo "Run the following commands:"
        [ -n "$MISSING_CMAKE" ] && echo "  sudo dnf install cmake"
        [ -n "$MISSING_COMPILER" ] && echo "  sudo dnf install gcc-c++"
        [ -n "$MISSING_SYSTEMD" ] && echo "  sudo dnf install systemd-devel"
    elif [ "$PKG_MANAGER" = "yum" ]; then
        echo "Run the following commands:"
        [ -n "$MISSING_CMAKE" ] && echo "  sudo yum install cmake"
        [ -n "$MISSING_COMPILER" ] && echo "  sudo yum install gcc-c++"
        [ -n "$MISSING_SYSTEMD" ] && echo "  sudo yum install systemd-devel"
    elif [ "$PKG_MANAGER" = "pacman" ]; then
        echo "Run the following commands:"
        [ -n "$MISSING_CMAKE" ] && echo "  sudo pacman -S cmake"
        [ -n "$MISSING_COMPILER" ] && echo "  sudo pacman -S base-devel"
        [ -n "$MISSING_SYSTEMD" ] && echo "  sudo pacman -S systemd"
    fi

    echo ""
fi
