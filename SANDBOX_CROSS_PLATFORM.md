# Cross-Platform Sandbox Guide

This guide explains how to run MULO with the plugin sandbox enabled on different operating systems.

## Overview

The MULO plugin sandbox provides security by intercepting and blocking potentially dangerous operations when called by sandboxed plugins:

- 📁 **File System**: Blocks unauthorized file creation, deletion, and directory operations
- 🌐 **Network**: Blocks socket creation, connections, and data transmission
- ⚙️ **Execution**: Blocks system commands and process creation

## Platform-Specific Instructions

### 🍎 macOS

**Prerequisites:**
- Built MULO project with sandbox support
- Sandbox library: `libsandbox_override.dylib`

**Running with Sandbox:**
```bash
./run_with_sandbox.sh
```

**Running without Sandbox:**
```bash
./run_with_sandbox.sh --no-sandbox
```

**Technical Details:**
- Uses `DYLD_INSERT_LIBRARIES` environment variable
- Intercepts system calls using `DYLD_INTERPOSE` macro
- May require disabling System Integrity Protection (SIP) for library injection
- Recommended to run from a non-protected location

**Building:**
```bash
cd build
cmake ..
cmake --build . --target sandbox_override
```

---

### 🐧 Linux

**Prerequisites:**
- Built MULO project with sandbox support
- Sandbox library: `libsandbox_override.so`

**Running with Sandbox:**
```bash
./run_with_sandbox.sh
```

**Running without Sandbox:**
```bash
./run_with_sandbox.sh --no-sandbox
```

**Technical Details:**
- Uses `LD_PRELOAD` environment variable
- Intercepts system calls using `dlsym` with `RTLD_NEXT`
- Works reliably on most Linux distributions

**Building:**
```bash
cd build
cmake ..
cmake --build . --target sandbox_override
```

---

### 🪟 Windows

**Prerequisites:**
- Built MULO project with sandbox support
- Sandbox library: `sandbox_override.dll`
- PowerShell (included in Windows 7+)

**Running with Sandbox (PowerShell):**
```powershell
.\run_with_sandbox.ps1
```

**Running with Sandbox (Command Prompt):**
```batch
run_sandbox.bat
```

**Running without Sandbox:**
```powershell
.\run_with_sandbox.ps1 -NoSandbox
```

**Specifying Build Type:**
```powershell
.\run_with_sandbox.ps1 -BuildType Debug
```

**Technical Details:**
- Windows doesn't have a direct equivalent to `LD_PRELOAD`
- Sandbox implementation may use:
  - **DLL Injection**: Loading the sandbox DLL into the process
  - **Detours**: Microsoft Detours library for API hooking
  - **Import Address Table (IAT) hooking**: Modifying function pointers
- The sandbox DLL must be in the same directory as `MULO.exe`

**Building:**
```powershell
cd build
cmake --build . --config Release --target sandbox_override
```

Or using Visual Studio:
```powershell
cmake --build . --config Debug --target sandbox_override
```

---

## Quick Start (Any Platform)

### macOS / Linux:
```bash
chmod +x run_with_sandbox.sh
./run_with_sandbox.sh
```

### Windows (PowerShell):
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\run_with_sandbox.ps1
```

### Windows (Batch):
```batch
run_sandbox.bat
```

---

## Build Configuration

The sandbox is automatically built for supported platforms when you build the MULO project:

```bash
# From project root
mkdir -p build
cd build
cmake ..
cmake --build .
```

The CMakeLists.txt automatically:
- ✅ Builds sandbox for macOS (Darwin)
- ✅ Builds sandbox for Linux
- ⚠️ Windows support may require additional configuration

---

## Environment Variables

### macOS:
- `DYLD_INSERT_LIBRARIES`: Path to the sandbox dylib

### Linux:
- `LD_PRELOAD`: Path to the sandbox .so file

### Windows:
- `MULO_SANDBOX_ENABLED`: Set to "1" to enable sandbox features

---

## Troubleshooting

### macOS Issues:

**Problem**: Sandbox doesn't intercept calls  
**Solution**: 
- Disable SIP: `csrutil disable` (requires reboot, not recommended for production)
- Run from user directory (not `/Applications`)
- Check if `DYLD_INSERT_LIBRARIES` is being respected

### Linux Issues:

**Problem**: Sandbox library not found  
**Solution**:
- Ensure library is built: `ls bin/Linux/Release/libsandbox_override.so`
- Check permissions: `chmod +x bin/Linux/Release/libsandbox_override.so`
- Verify LD_PRELOAD path is absolute

### Windows Issues:

**Problem**: PowerShell execution policy prevents script  
**Solution**:
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

**Problem**: Sandbox DLL not found  
**Solution**:
- Build the sandbox target specifically
- Check that DLL is in same directory as MULO.exe
- Script automatically copies DLL if needed

**Problem**: DLL fails to load  
**Solution**:
- Ensure Visual C++ Redistributable is installed
- Check DLL architecture matches (x64 vs x86)
- Verify all dependencies are available

---

## Monitoring Sandbox Activity

When the sandbox is active, you'll see console output like:

```
[SANDBOX] BLOCKED open() by plugin: MyPlugin.vst3 (path: /etc/passwd)
[SANDBOX] BLOCKED socket() by plugin: SuspiciousPlugin.vst3
[SANDBOX] BLOCKED system() by plugin: MaliciousPlugin.vst3 (command: rm -rf /)
```

This indicates the sandbox is working correctly and blocking unauthorized operations.

---

## Development Notes

### Adding Windows Sandbox Support to CMakeLists.txt

If Windows sandbox isn't building automatically, you may need to add:

```cmake
if (WIN32)
    add_library(sandbox_override SHARED 
        src/frontend/sandbox_override_windows.cpp 
        src/frontend/PluginSandbox.cpp
    )
    target_include_directories(sandbox_override PRIVATE src/frontend)
    
    set_target_properties(sandbox_override PROPERTIES
        OUTPUT_NAME "sandbox_override"
        SUFFIX ".dll"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}
    )
    
    # Link Windows-specific libraries
    target_link_libraries(sandbox_override PRIVATE 
        kernel32
        advapi32
        ws2_32
    )
    
    message(STATUS "Sandbox library will be built for Windows (DLL Injection)")
endif()
```

### Platform Detection Summary

| Platform | Script | Library | Method |
|----------|--------|---------|--------|
| macOS | `run_with_sandbox.sh` | `libsandbox_override.dylib` | `DYLD_INSERT_LIBRARIES` |
| Linux | `run_with_sandbox.sh` | `libsandbox_override.so` | `LD_PRELOAD` |
| Windows | `run_with_sandbox.ps1` | `sandbox_override.dll` | DLL Injection / Detours |

---

## Security Considerations

1. **Legitimate Operations**: The sandbox allows certain system paths for audio plugins to function normally (e.g., `/Library/Audio/`, `CoreAudio`, VST3 bundles)

2. **Thread Safety**: The sandbox tracks plugins per-thread, allowing multiple plugins to run simultaneously

3. **Performance**: The sandbox adds minimal overhead as it only checks calls from sandboxed plugins

4. **Bypass Risk**: Advanced plugins may be able to bypass the sandbox through:
   - Direct system calls (bypassing libc)
   - JIT compilation
   - Memory manipulation
   - Kernel-mode drivers (Windows)

5. **Production Use**: The sandbox is meant as a development/testing tool. For production security, use OS-level sandboxing:
   - macOS: App Sandbox entitlements
   - Linux: AppArmor, SELinux, seccomp
   - Windows: AppContainer, Windows Sandbox

---

## License

This sandbox implementation is part of the MULO project.
