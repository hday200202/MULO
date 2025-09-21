#!/bin/bash
set -e

echo "Starting MULO in container..."

# Start Xvfb virtual display (ignore warnings)
echo "Starting virtual display..."
Xvfb :99 -screen 0 1024x768x24 -ac > /tmp/xvfb.log 2>&1 &
XVFB_PID=$!

# Wait for X server to start
sleep 3

# Set display
export DISPLAY=:99

echo "Virtual display ready on :99"

# Show current environment
echo "Environment:"
echo "  DISPLAY=$DISPLAY"
echo "  LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "  Working directory: $(pwd)"

# List available files
echo "Available files:"
ls -la /app/

# Check config file
echo "Config file contents:"
cat /app/config.json

# Run MULO with error handling
echo "Starting MULO..."
exec /app/MULO