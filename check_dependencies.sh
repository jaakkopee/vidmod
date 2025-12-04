#!/bin/bash

# Dependency check script for FFT Video Modulator

echo "=== Checking Dependencies ==="
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

check_command() {
    if command -v "$1" &> /dev/null; then
        echo -e "${GREEN}✓${NC} $1 found"
        return 0
    else
        echo -e "${RED}✗${NC} $1 not found"
        return 1
    fi
}

check_pkg_config() {
    if pkg-config --exists "$1" 2>/dev/null; then
        version=$(pkg-config --modversion "$1" 2>/dev/null)
        echo -e "${GREEN}✓${NC} $1 found (version: $version)"
        return 0
    else
        echo -e "${RED}✗${NC} $1 not found"
        return 1
    fi
}

# Check basic tools
echo "Basic Tools:"
check_command cmake
check_command make
check_command pkg-config
echo ""

# Check libraries via pkg-config
echo "Required Libraries:"
check_pkg_config sfml-all || check_pkg_config sfml-graphics
check_pkg_config tgui
check_pkg_config fftw3
check_pkg_config opencv4 || check_pkg_config opencv
check_pkg_config sndfile
echo ""

# Summary
echo "=== Dependency Check Complete ==="
echo ""
echo "If any dependencies are missing, install them with:"
echo "  macOS: brew install sfml tgui fftw opencv libsndfile pkg-config"
echo "  Ubuntu/Debian: sudo apt-get install libsfml-dev libtgui-dev libfftw3-dev libopencv-dev libsndfile1-dev pkg-config"
echo ""
