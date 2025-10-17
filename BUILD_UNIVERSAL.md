# Building MULO as Universal Binary (arm64 + x86_64)

## Prerequisites
You need universal versions of all dependencies:
- SFML (universal)
- JUCE (already supports universal)
- All other libraries

## Build Steps

```bash
cd /Users/prashantneupane/Desktop/test_mdaw/MDAW/build

# Clean previous build
rm -rf *

# Configure for universal binary
cmake -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" ..

# Build
cmake --build . -j8
```

## Note
This requires all dependencies (SFML, Firebase, etc.) to be available as universal binaries.
If you get linker errors, you'll need to install universal versions of those libraries.

## Quick Check
After building, verify it's universal:
```bash
lipo -info bin/Darwin/Release/MULO
# Should show: Architectures in the fat file: arm64 x86_64
```
