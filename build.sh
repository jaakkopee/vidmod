#!/bin/bash

# Build script for FFT Video Modulator

set -e  # Exit on error

echo "=== FFT Video Modulator Build Script ==="
echo ""

# Check if dependencies are installed
echo "Checking dependencies..."

command -v pkg-config >/dev/null 2>&1 || {
    echo "Error: pkg-config is required but not installed."
    exit 1
}

# Create build directory
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build
echo "Building..."
make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

echo ""
echo "=== Build Complete ==="
echo "Executable: build/bin/FFTVidMod"
echo ""
echo "To run the application:"
echo "  cd build/bin"
echo "  ./FFTVidMod"
echo ""
