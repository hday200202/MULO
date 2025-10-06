#!/usr/bin/env bash

# Exit the script immediately if any command fails
set -e

echo "Updating Git submodules..."
git submodule update --init --recursive

BUILD_TYPE="Debug"
BUILD_DIR="build"
DO_CLEAN=0
BUILD_ALL=0

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
        all|All)
            BUILD_ALL=1
            ;;
        release|Release)
            BUILD_TYPE="Release"
            ;;
        debug|Debug)
            BUILD_TYPE="Debug"
            ;;
        rdbi|RDBI|RelWithDebInfo)
            BUILD_TYPE="RelWithDebInfo"
            ;;
        msr|MSR|MinSizeRel)
            BUILD_TYPE="MinSizeRel"
            ;;
        clean|Clean)
            DO_CLEAN=1
            ;;
    esac
done

# Function to build extensions
build_extensions() {
    echo "Building extensions_toolkit ($BUILD_TYPE)..."
    cd extensions_toolkit/
    ./build.sh $BUILD_TYPE $([ $DO_CLEAN -eq 1 ] && echo "clean")
    cd ..
}

# Function to build main project
build_main() {
    if [ $DO_CLEAN -eq 1 ]; then
        echo "Cleaning main build directory..."
        rm -rf "$BUILD_DIR"
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

    echo "Building main project ($BUILD_TYPE)..."
    cmake --build "$BUILD_DIR" --config $BUILD_TYPE -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)
}

# Handle build logic
if [ $BUILD_ALL -eq 1 ]; then
    echo "Building all projects..."
    build_extensions
    build_main
elif [ $DO_CLEAN -eq 1 ] && [ $# -eq 1 ]; then
    # Only clean was requested
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    rm -rf "VSTEditor/build"
    exit 0
else
    # Build only main project
    build_main
fi

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