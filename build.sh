#!/usr/bin/env bash

# Exit the script immediately if any command fails
set -e

echo "Updating Git submodules..."
git submodule update --init --recursive

BUILD_TYPE="Debug"
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
echo "Configuring build ($BUILD_TYPE)..."

    # Platform-specific CMake configuration
    if [ "$PLATFORM" = "Windows" ]; then
        # Force 64-bit build on Windows using Visual Studio generator
        if command -v ninja &> /dev/null; then
            echo "Using Ninja generator for faster builds"
            cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -A x64 -G Ninja
        else
            echo "Using Visual Studio generator with 64-bit architecture"
            cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -A x64
        fi
    else
        # Use Ninja if available for faster builds on Linux/Darwin
        if command -v ninja &> /dev/null; then
            echo "Using Ninja generator for faster builds"
            cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_LINKER=lld -G Ninja
        else
            cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE
        fi
    fi

echo "Building ($BUILD_TYPE)..."
cmake --build "$BUILD_DIR" --config $BUILD_TYPE -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)

echo "Done building."

# Determine executable path
case "$PLATFORM" in
    Windows)
        EXE_PATH="bin/Windows/$BUILD_TYPE/MULO.exe"
        ;;
    Darwin)
        EXE_PATH="bin/Darwin/$BUILD_TYPE/MULO"
        ;;
    Linux)
        EXE_PATH="bin/Linux/$BUILD_TYPE/MULO"
        ;;
    *)
        echo "Unknown platform, cannot determine executable path."
        exit 1
        ;;
esac

if [[ -f "$EXE_PATH" ]]; then
    echo "Running $EXE_PATH"
    EXE_DIR=$(dirname "$EXE_PATH")
    EXE_FILE=$(basename "$EXE_PATH")
    pushd "$EXE_DIR" > /dev/null
    chmod +x "$EXE_FILE"
    "./$EXE_FILE"
    popd > /dev/null
else
    echo "Executable not found: $EXE_PATH"
fi