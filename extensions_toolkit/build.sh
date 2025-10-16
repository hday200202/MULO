#!/usr/bin/env bash

# Exit the script immediately if any command fails
set -e

BUILD_TYPE="Release"
BUILD_DIR="build"
DO_CLEAN=0
HASH_DIR=".build_hashes"
FORCE_REBUILD_ALL=0

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
        rdbi|RDBI|RelWithDebInfo)
            BUILD_TYPE="RelWithDebInfo"
            ;;
        msr|MSR|MinSizeRel)
            BUILD_TYPE="MinSizeRel"
            ;;
        clean|Clean)
            DO_CLEAN=1
            ;;
        all)
            FORCE_REBUILD_ALL=1
            ;;
    esac
done

if [ $DO_CLEAN -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    rm -rf "$HASH_DIR"
    if [ $# -eq 1 ]; then
        exit 0
    fi
fi

# Create hash directory if it doesn't exist
mkdir -p "$HASH_DIR"

# Function to compute hash of a file
compute_hash() {
    if command -v md5sum &> /dev/null; then
        md5sum "$1" | awk '{print $1}'
    elif command -v md5 &> /dev/null; then
        md5 -q "$1"
    else
        echo "ERROR: No hash command available (md5sum or md5)"
        exit 1
    fi
}

# Function to check if extension needs rebuild
needs_rebuild() {
    local extension="$1"
    local hpp_file="extensions_src/${extension}.hpp"
    local cpp_file="extensions_src/${extension}.cpp"
    local hash_file="${HASH_DIR}/${extension}.hash"
    
    # If force rebuild all, return true
    if [ $FORCE_REBUILD_ALL -eq 1 ]; then
        return 0
    fi
    
    # If hash file doesn't exist, needs rebuild
    if [ ! -f "$hash_file" ]; then
        return 0
    fi
    
    # Compute current hash of extension files
    local current_hash=""
    if [ -f "$hpp_file" ]; then
        current_hash+=$(compute_hash "$hpp_file")
    fi
    if [ -f "$cpp_file" ]; then
        current_hash+=$(compute_hash "$cpp_file")
    fi
    
    # Compare with stored hash
    local stored_hash=$(cat "$hash_file" 2>/dev/null || echo "")
    if [ "$current_hash" != "$stored_hash" ]; then
        return 0
    fi
    
    return 1
}

# Function to save hash for extension
save_hash() {
    local extension="$1"
    local hpp_file="extensions_src/${extension}.hpp"
    local cpp_file="extensions_src/${extension}.cpp"
    local hash_file="${HASH_DIR}/${extension}.hash"
    
    local current_hash=""
    if [ -f "$hpp_file" ]; then
        current_hash+=$(compute_hash "$hpp_file")
    fi
    if [ -f "$cpp_file" ]; then
        current_hash+=$(compute_hash "$cpp_file")
    fi
    
    echo "$current_hash" > "$hash_file"
}

# Check if frontend/audio files changed (these should trigger rebuild of all extensions)
check_engine_changes() {
    local engine_hash_file="${HASH_DIR}/engine_sources.hash"
    local current_engine_hash=""
    
    # Hash all frontend and audio source files
    for file in ../src/frontend/*.cpp ../src/frontend/*.hpp ../src/audio/*.cpp ../src/audio/*.hpp; do
        if [ -f "$file" ]; then
            current_engine_hash+=$(compute_hash "$file")
        fi
    done
    
    local stored_engine_hash=$(cat "$engine_hash_file" 2>/dev/null || echo "")
    
    if [ "$current_engine_hash" != "$stored_engine_hash" ]; then
        echo "Engine sources changed, forcing rebuild of all extensions..."
        FORCE_REBUILD_ALL=1
        echo "$current_engine_hash" > "$engine_hash_file"
    fi
}

echo "Platform detected: $PLATFORM"

# Check for engine changes
check_engine_changes

# Check which extensions need rebuilding and create a list file
echo "Checking which extensions need rebuilding..."
EXTENSIONS_TO_BUILD=""
BUILD_LIST_FILE="$BUILD_DIR/extensions_to_build.txt"
mkdir -p "$BUILD_DIR"
> "$BUILD_LIST_FILE"  # Clear the file

for hpp_file in extensions_src/*.hpp; do
    if [ -f "$hpp_file" ]; then
        extension=$(basename "$hpp_file" .hpp)
        if needs_rebuild "$extension"; then
            echo "  - $extension (changed or new)"
            EXTENSIONS_TO_BUILD+="$extension "
            echo "$extension" >> "$BUILD_LIST_FILE"
        else
            echo "  - $extension (unchanged, skipping)"
        fi
    fi
done

if [ -z "$EXTENSIONS_TO_BUILD" ]; then
    echo "No extensions need rebuilding!"
    exit 0
fi

echo "Configuring extensions build ($BUILD_TYPE)..."

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
    # Use Ninja if available for faster builds on Linux/Mac
    if command -v ninja &> /dev/null; then
        echo "Using Ninja generator for faster builds"
        cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_LINKER=lld -G Ninja
    else
        cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=$BUILD_TYPE
    fi
fi

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
    echo "Looking for alternative build output directories..."
    # Check if CMake put files in the build directory instead
    BUILD_EXTENSIONS_PATH="$BUILD_DIR/$BUILD_TYPE"
    if [[ -d "$BUILD_EXTENSIONS_PATH" ]]; then
        echo "Found extensions in build directory: $BUILD_EXTENSIONS_PATH"
        EXTENSIONS_PATH="$BUILD_EXTENSIONS_PATH"
    else
        echo "No extensions found in expected locations"
        echo "Available directories:"
        find . -type d -name "*$BUILD_TYPE*" 2>/dev/null || echo "No Release directories found"
    fi
fi

# Copy extensions to ../bin/$PLATFORM/$BUILD_TYPE/extensions
DEST_DIR="../bin/$PLATFORM/$BUILD_TYPE/extensions"
echo "Copying extensions to $DEST_DIR"
mkdir -p "$DEST_DIR"

# Copy platform-specific extension files if the source directory exists
if [[ -d "$EXTENSIONS_PATH" ]]; then
    if [[ "$PLATFORM" == "Windows" ]]; then
        # Copy .dll files on Windows
        if find "$EXTENSIONS_PATH" -maxdepth 1 -type f -name "*.dll" -print -quit | grep -q .; then
            find "$EXTENSIONS_PATH" -maxdepth 1 -type f -name "*.dll" -exec cp {} "$DEST_DIR" \;
            echo "Copied .dll files to $DEST_DIR"
        else
            echo "No .dll files found in $EXTENSIONS_PATH"
        fi
    elif [[ "$PLATFORM" == "Darwin" ]]; then
        # Copy .dylib files on macOS
        if find "$EXTENSIONS_PATH" -maxdepth 1 -type f -name "*.dylib" -print -quit | grep -q .; then
            find "$EXTENSIONS_PATH" -maxdepth 1 -type f -name "*.dylib" -exec cp {} "$DEST_DIR" \;
            echo "Copied .dylib files to $DEST_DIR"
        else
            echo "No .dylib files found in $EXTENSIONS_PATH"
        fi
    else
        # Copy .so files on Linux
        if find "$EXTENSIONS_PATH" -maxdepth 1 -type f -name "*.so" -print -quit | grep -q .; then
            find "$EXTENSIONS_PATH" -maxdepth 1 -type f -name "*.so" -exec cp {} "$DEST_DIR" \;
            echo "Copied .so files to $DEST_DIR"
        else
            echo "No .so files found in $EXTENSIONS_PATH"
        fi
    fi
else
    echo "Source extensions directory $EXTENSIONS_PATH does not exist, skipping copy"
fi

echo "Extensions build process completed."

# Save hashes for successfully built extensions
echo "Updating build hashes..."
for extension in $EXTENSIONS_TO_BUILD; do
    save_hash "$extension"
done

echo "Build complete!"
