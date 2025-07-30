#!/usr/bin/env bash

# Exit the script immediately if any command fails
set -e

BUILD_TYPE="Release"
BUILD_DIR="build"
DO_CLEAN=0

# Detect platform
UNAME_OUT="$(uname -s)"
case "${UNAME_OUT}" in
    Linux*)     PLATFORM="Linux";;
    Darwin*)    PLATFORM="Darwin";;
    CYGWIN*|MINGW*|MSYS*) PLATFORM="Windows";;
    *)          PLATFORM="Unknown";;
esac

# Parse arguments
for arg in "$@"; do
    case "$arg" in
        release|Release)
            BUILD_TYPE="Release"
            ;;
        debug|Debug)
            BUILD_TYPE="Debug"
            ;;
        rdbi|RDBI)
            BUILD_TYPE="RelWithDebInfo"
            ;;
        clean|Clean)
            DO_CLEAN=1
            ;;
    esac
done

if [ $DO_CLEAN -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    if [ $# -eq 1 ]; then
        exit 0
    fi
fi

echo "Platform detected: $PLATFORM"
echo "Configuring extensions build ($BUILD_TYPE)..."
cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE

echo "Building extensions ($BUILD_TYPE)..."
cmake --build "$BUILD_DIR" --config $BUILD_TYPE -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)

echo "Done building extensions."

# Show built extensions
EXTENSIONS_PATH="bin/$PLATFORM/$BUILD_TYPE"
if [[ -d "$EXTENSIONS_PATH" ]]; then
    echo "Extensions built in: $EXTENSIONS_PATH"
    ls -la "$EXTENSIONS_PATH"
else
    echo "Extensions directory not found: $EXTENSIONS_PATH"
fiO Extensions Build Script
echo "Building MULO Extensions..."

# Create build directory if it doesn't exist
mkdir -p build

# Navigate to build directory
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the extensions
echo "Building extensions..."
make -j$(nproc)
