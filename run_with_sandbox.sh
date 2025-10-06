#!/bin/bash

# Sandbox Test Script for MDAW
# This script demonstrates how to run MULO with the plugin sandbox enabled

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_DIR="$SCRIPT_DIR"

# Detect the platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="Darwin"
    SANDBOX_LIB="libsandbox_override.dylib"
    ENV_VAR="DYLD_INSERT_LIBRARIES"
    echo "🍎 Detected macOS platform"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    SANDBOX_LIB="libsandbox_override.so"
    ENV_VAR="LD_PRELOAD"
    echo "🐧 Detected Linux platform"
else
    echo "❌ Unsupported platform: $OSTYPE"
    exit 1
fi

# Find the sandbox library
BUILD_TYPE="${BUILD_TYPE:-Release}"
SANDBOX_PATH="$PROJECT_DIR/bin/$PLATFORM/$BUILD_TYPE/$SANDBOX_LIB"

if [ ! -f "$SANDBOX_PATH" ]; then
    echo "❌ Sandbox library not found at: $SANDBOX_PATH"
    echo "Please build the project first:"
    echo "  cd build && cmake --build . --target sandbox_override"
    exit 1
fi

echo "✅ Found sandbox library: $SANDBOX_PATH"

# Find the MULO executable
MULO_PATH="$PROJECT_DIR/bin/$PLATFORM/$BUILD_TYPE/MULO"

if [ ! -f "$MULO_PATH" ]; then
    echo "❌ MULO executable not found at: $MULO_PATH"
    echo "Please build the project first:"
    echo "  cd build && cmake --build ."
    exit 1
fi

echo "✅ Found MULO executable: $MULO_PATH"

# Display information
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  MDAW Plugin Sandbox Test"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Platform:        $PLATFORM"
echo "Build Type:      $BUILD_TYPE"
echo "Sandbox Library: $SANDBOX_LIB"
echo "Environment Var: $ENV_VAR"
echo ""
echo "The sandbox will intercept and block the following operations"
echo "when called by sandboxed plugins:"
echo ""
echo "  📁 File System: open, fopen, creat, unlink, mkdir, rmdir"
echo "  🌐 Network:     socket, connect, bind, listen, accept, send, recv"
echo "  ⚙️  Execution:   system, execve, fork, vfork"
echo ""
echo "Monitor the console output for [SANDBOX] BLOCKED messages"
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Option to run with or without sandbox
if [ "$1" == "--no-sandbox" ]; then
    echo "🔓 Running MULO WITHOUT sandbox (normal mode)"
    echo ""
    "$MULO_PATH"
else
    echo "🔒 Running MULO WITH sandbox enabled"
    echo ""
    echo "To run without sandbox, use: $0 --no-sandbox"
    echo ""
    
    # Set the environment variable and run MULO
    if [[ "$PLATFORM" == "Darwin" ]]; then
        # On macOS, we may need to disable library validation
        echo "Note: On macOS, if the sandbox doesn't work, you may need to:"
        echo "  1. Disable SIP (System Integrity Protection) - not recommended for production"
        echo "  2. Sign the MULO binary with appropriate entitlements"
        echo "  3. Run from a non-protected location"
        echo ""
        export DYLD_INSERT_LIBRARIES="$SANDBOX_PATH"
    else
        export LD_PRELOAD="$SANDBOX_PATH"
    fi
    
    "$MULO_PATH"
fi
