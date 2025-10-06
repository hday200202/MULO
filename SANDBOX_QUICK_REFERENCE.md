# Plugin Sandbox Quick Reference

## ⚡ Quick Start

### Build
```bash
cd build
cmake .. && cmake --build . --target sandbox_override
```

### Run
```bash
# Use the helper script
./run_with_sandbox.sh

# Or manually set environment:
# macOS:
export DYLD_INSERT_LIBRARIES=./bin/Darwin/Release/libsandbox_override.dylib
# Linux:
export LD_PRELOAD=./bin/Linux/Release/libsandbox_override.so

./bin/*/Release/MULO
```

## 🔒 In Your Code

```cpp
#include "PluginSandbox.hpp"

// Enable sandbox for a specific plugin
PluginSandbox::enableSandbox("UntrustedPlugin.vst3");

// Load and use the plugin...

// Disable when done
PluginSandbox::disableSandbox();
```

## 🛡️ What Gets Blocked

| Category | Functions |
|----------|-----------|
| **Files** | `open(write)`, `fopen(w/a)`, `creat`, `unlink`, `mkdir`, `rmdir` |
| **Network** | `socket`, `connect`, `bind`, `listen`, `accept`, `send*`, `recv*` |
| **Execution** | `system`, `exec*`, `fork`, `vfork` |

## 📋 Platform Differences

| Feature | macOS | Linux |
|---------|-------|-------|
| **Injection** | `DYLD_INSERT_LIBRARIES` | `LD_PRELOAD` |
| **Library** | `.dylib` | `.so` |
| **Technique** | `DYLD_INTERPOSE` | `dlsym(RTLD_NEXT)` |
| **Plugin Format** | `.vst3`, `.dylib` | `.so` |

## 🐛 Debugging

Watch for console output:
```
[SANDBOX] BLOCKED system() for plugin 'Evil.vst3': rm -rf /
[SANDBOX] BLOCKED socket() for plugin 'Spy.vst3'
```

## ⚠️ Important Notes

### macOS
- May need to disable SIP for testing
- Don't run as root with SIP disabled
- For distribution: proper code signing required

### Linux  
- Won't work with setuid binaries
- May need to adjust security policies
- Ensure `sandbox_override.so` is in library path

## 📖 Full Documentation

See `src/frontend/SANDBOX_README.md` for complete documentation.
