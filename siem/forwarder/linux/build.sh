#!/bin/bash
# Automatic compilation script for Linux System Log Forwarder

echo "========================================"
echo "Linux System Log Forwarder Builder"
echo "========================================"
echo ""

# Check if CMake is installed
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake is not installed"
    echo "Install with: sudo apt install cmake"
    exit 1
fi

# Check for libsystemd
if ! pkg-config --exists libsystemd; then
    echo "ERROR: libsystemd not found"
    echo "Install with: sudo apt install libsystemd-dev"
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Generate build files
echo "Generating build files..."
cmake ..
if [ $? -ne 0 ]; then
    echo "ERROR: CMake configuration failed"
    cd ..
    exit 1
fi

# Build the project
echo ""
echo "Building project..."
make -j$(nproc)
if [ $? -ne 0 ]; then
    echo "ERROR: Build failed"
    cd ..
    exit 1
fi

echo ""
echo "========================================"
echo "Build completed successfully!"
echo "Executables: bin/log_forwarder"
echo "             bin/test_forwarder"
echo "========================================"
echo ""

cd ..
