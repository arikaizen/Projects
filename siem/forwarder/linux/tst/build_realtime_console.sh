#!/bin/bash
# ========================================
# Build script for Real-Time Console Test
# ========================================

echo "========================================"
echo "Building Real-Time Console Test Program"
echo "========================================"
echo ""

# Check if g++ is available
if ! command -v g++ &> /dev/null; then
    echo "[ERROR] g++ not found"
    echo "[ERROR] Please install g++ (build-essential on Debian/Ubuntu)"
    exit 1
fi

# Check if libsystemd-dev is available
if ! pkg-config --exists libsystemd 2>/dev/null; then
    echo "[WARNING] libsystemd-dev not found"
    echo "[WARNING] Install with: sudo apt-get install libsystemd-dev (Debian/Ubuntu)"
    echo "[WARNING]           or: sudo dnf install systemd-devel (RHEL/Fedora)"
    echo ""
fi

echo "[1/3] Checking directories..."
if [ ! -d "../bin" ]; then
    echo "Creating bin directory..."
    mkdir -p "../bin"
fi

echo ""
echo "[2/3] Compiling test_realtime_console.cpp..."
g++ -I../inc \
    test_realtime_console.cpp \
    ../src/journal_reader.cpp \
    ../src/json_utils.cpp \
    -o ../bin/test_realtime_console \
    $(pkg-config --cflags --libs libsystemd) \
    -std=c++17 \
    -Wall \
    -lpthread

if [ $? -ne 0 ]; then
    echo "[ERROR] Compilation failed"
    exit 1
fi

echo ""
echo "[3/3] Build complete!"
echo "========================================"
echo ""
echo "Executable created: ../bin/test_realtime_console"
echo ""
echo "Usage Examples:"
echo "  ../bin/test_realtime_console"
echo "  ../bin/test_realtime_console journal realtime"
echo "  ../bin/test_realtime_console auth recent"
echo "  ../bin/test_realtime_console syslog all"
echo ""
echo "Run '../bin/test_realtime_console --help' for full usage"
echo ""
echo "Note: Some log sources may require sudo/root privileges"
echo ""
echo "========================================"
