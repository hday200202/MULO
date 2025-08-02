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

if command -v ninja &> /dev/null; then
    echo "Using Ninja generator for faster builds"
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_LINKER=lld -G Ninja
else
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE
fi

echo "Building extensions ($BUILD_TYPE)..."
cmake --build "$BUILD_DIR" --config $BUILD_TYPE -j $(nproc 2>/dev/null || sysctl -n hw.ncpu)

echo "Done building extensions."

# Show built extensions
EXTENSIONS_PATH="build/$BUILD_TYPE"
if [[ -d "$EXTENSIONS_PATH" ]]; then
    echo "Extensions built in: $EXTENSIONS_PATH"
    ls -la "$EXTENSIONS_PATH"
else
    echo "Extensions directory not found: $EXTENSIONS_PATH"
fi

# Copy extensions to ../bin/$PLATFORM/$BUILD_TYPE/extensions
DEST_DIR="../bin/$PLATFORM/$BUILD_TYPE/extensions"
echo "Copying extensions to $DEST_DIR"
mkdir -p "$DEST_DIR"

# Copy platform-specific extension files
if [[ "$PLATFORM" == "Windows" ]]; then
    # Copy .dll files on Windows
    find "$EXTENSIONS_PATH" -maxdepth 1 -type f -name "*.dll" -exec cp {} "$DEST_DIR" \;
    echo "Copied .dll files to $DEST_DIR"
elif [[ "$PLATFORM" == "Darwin" ]]; then
    # Copy .dylib files on macOS
    find "$EXTENSIONS_PATH" -maxdepth 1 -type f -name "*.dylib" -exec cp {} "$DEST_DIR" \;
    echo "Copied .dylib files to $DEST_DIR"
else
    # Copy .so files on Linux
    find "$EXTENSIONS_PATH" -maxdepth 1 -type f -name "*.so" -exec cp {} "$DEST_DIR" \;
    echo "Copied .so files to $DEST_DIR"
fi

echo "Extensions copied to $DEST_DIR"
