#!/bin/bash
# MDAW Development Workflow Script

set -e

echo "🎵 MDAW Development Workflow"
echo "=========================="

# Function to build native
build_native() {
    echo "Building MULO natively..."
    ./build.sh release
    echo "✅ Native build complete"
    
    # Ensure Debug build has necessary files
    ensure_debug_assets
}

# Function to build debug
build_debug() {
    echo "Building MULO in debug mode..."
    ./build.sh debug
    echo "✅ Debug build complete"
    
    # Ensure Debug build has necessary files
    ensure_debug_assets
}

# Function to ensure debug assets exist
ensure_debug_assets() {
    if [[ -d "bin/Linux/Release" && -d "bin/Linux/Debug" ]]; then
        echo "Ensuring Debug build has necessary assets..."
        
        # Copy missing files from Release to Debug
        [[ ! -f "bin/Linux/Debug/layout.json" && -f "bin/Linux/Release/layout.json" ]] && \
            cp bin/Linux/Release/layout.json bin/Linux/Debug/
            
        [[ ! -f "bin/Linux/Debug/config.json" && -f "bin/Linux/Release/config.json" ]] && \
            cp bin/Linux/Release/config.json bin/Linux/Debug/
            
        [[ ! -d "bin/Linux/Debug/extensions" && -d "bin/Linux/Release/extensions" ]] && \
            cp -r bin/Linux/Release/extensions bin/Linux/Debug/
            
        echo "✅ Debug assets synchronized"
    fi
}

# Function to build container
build_container() {
    echo "Building container..."
    podman build -f containers/Containerfile.runtime -t mulo:runtime .
    echo "✅ Container build complete"
}

# Function to run native
run_native() {
    echo "Running MULO natively..."
    if [[ -f "bin/Linux/Release/MULO" ]]; then
        cd bin/Linux/Release
        # Set MIDI environment to prevent JUCE assertion failures
        export JUCE_DISABLE_AUDIO_DEVICE_SCANNING=1
        ./MULO
    else
        echo "❌ MULO not built. Run: $0 build"
        exit 1
    fi
}

# Function to run debug build
run_debug() {
    echo "Running MULO in debug mode..."
    if [[ -f "bin/Linux/Debug/MULO" ]]; then
        cd bin/Linux/Debug
        # Set MIDI environment to prevent JUCE assertion failures
        export JUCE_DISABLE_AUDIO_DEVICE_SCANNING=1
        ./MULO
    else
        echo "❌ Debug MULO not built. Run: $0 build-debug"
        exit 1
    fi
}

# Function to run container
run_container() {
    echo "Running MULO in container..."
    podman run --rm mulo:runtime
}

# Function to run container interactively
debug_container() {
    echo "Starting interactive container for debugging..."
    podman run --rm -it --user root --entrypoint="/bin/bash" mulo:runtime
}

# Main logic
case "$1" in
    "build")
        build_native
        build_container
        ;;
    "run")
        run_native
        ;;
    "container")
        run_container
        ;;
    "debug")
        debug_container
        ;;
    "clean")
        echo "Cleaning build artifacts..."
        rm -rf build/
        rm -rf bin/
        echo "✅ Clean complete"
        ;;
    "help"|"")
        echo "Usage: $0 <command>"
        echo ""
        echo "Commands:"
        echo "  build     - Build MULO natively and create container"
        echo "  run       - Run MULO natively"
        echo "  container - Run MULO in sandbox container"
        echo "  debug     - Open interactive container shell"
        echo "  clean     - Clean build artifacts"
        echo "  help      - Show this help"
        echo ""
        echo "Examples:"
        echo "  $0 build      # Build everything"
        echo "  $0 run        # Run for development"
        echo "  $0 container  # Test in sandbox"
        ;;
    *)
        echo "❌ Unknown command: $1"
        echo "Run '$0 help' for usage"
        exit 1
        ;;
esac