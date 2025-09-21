#!/bin/bash
# Quick GDB debugging script for MULO

cd /home/prashant/Desktop/MDAW/bin/Linux/Debug

export JUCE_DISABLE_AUDIO_DEVICE_SCANNING=1

echo "Starting MULO with GDB..."
echo "Running 'run', 'bt' and 'quit' automatically..."

gdb -batch -ex "run" -ex "bt" -ex "quit" ./MULO