# MULO Sandbox Launcher Scripts

This directory contains scripts to run MULO with the plugin sandbox enabled on different platforms.

## Quick Start

### Windows
```batch
run_sandbox.bat
```
Or:
```powershell
.\run_with_sandbox.ps1
```

### macOS / Linux
```bash
./run_with_sandbox.sh
```

## Available Scripts

| Script | Platform | Description |
|--------|----------|-------------|
| `run_with_sandbox.sh` | macOS, Linux | Bash script using LD_PRELOAD (Linux) or DYLD_INSERT_LIBRARIES (macOS) |
| `run_with_sandbox.ps1` | Windows | PowerShell script for Windows with DLL support |
| `run_sandbox.bat` | Windows | Batch file wrapper that calls the PowerShell script |
| `launch_sandbox.ps1` | All | Universal launcher (cross-platform PowerShell) |

## Platform-Specific Details

### macOS 🍎
- Uses `DYLD_INSERT_LIBRARIES` environment variable
- Library: `libsandbox_override.dylib`
- Location: `bin/Darwin/Release/`

**Usage:**
```bash
./run_with_sandbox.sh              # With sandbox
./run_with_sandbox.sh --no-sandbox # Without sandbox
```

### Linux 🐧
- Uses `LD_PRELOAD` environment variable
- Library: `libsandbox_override.so`
- Location: `bin/Linux/Release/`

**Usage:**
```bash
./run_with_sandbox.sh              # With sandbox
./run_with_sandbox.sh --no-sandbox # Without sandbox
```

### Windows 🪟
- Uses DLL in same directory as executable
- Library: `sandbox_override.dll`
- Location: `bin/Windows/Release/`

**Usage:**
```powershell
.\run_with_sandbox.ps1                    # With sandbox (Release)
.\run_with_sandbox.ps1 -NoSandbox         # Without sandbox
.\run_with_sandbox.ps1 -BuildType Debug   # With sandbox (Debug)
```

Or using batch file:
```batch
run_sandbox.bat
```

## What Does the Sandbox Do?

The plugin sandbox intercepts and blocks potentially dangerous system calls made by VST3 plugins:

### Blocked Operations
- **File System**: `open`, `fopen`, `creat`, `unlink`, `mkdir`, `rmdir`, `remove`, `rename`
- **Network**: `socket`, `connect`, `bind`, `listen`, `accept`, `send`, `recv`
- **Process Execution**: `system`, `execve`, `fork`, `vfork`, `popen`

### Allowed Operations
- Audio-related system paths (e.g., `/Library/Audio/`, `CoreAudio`)
- VST3 bundle resources
- Temporary directories for legitimate audio processing
- Plugin configuration files

## Console Output

When the sandbox is active, you'll see messages like:

```
[SANDBOX] BLOCKED open() by plugin: MyPlugin.vst3 (path: /etc/passwd)
[SANDBOX] BLOCKED socket() by plugin: NetworkPlugin.vst3
[SANDBOX] BLOCKED system() by plugin: MaliciousPlugin.vst3 (command: rm -rf /)
```

These indicate the sandbox is working correctly.

## Building the Sandbox

The sandbox library is built automatically when you build MULO:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

To build only the sandbox:

**macOS/Linux:**
```bash
cmake --build build --target sandbox_override
```

**Windows:**
```powershell
cmake --build build --config Release --target sandbox_override
```

## Troubleshooting

### macOS: "sandbox library not working"
- Disable System Integrity Protection (SIP): `csrutil disable` (requires reboot)
- Run from a user directory, not `/Applications`
- Check that `DYLD_INSERT_LIBRARIES` is set correctly

### Linux: "cannot preload library"
- Ensure library has execute permissions: `chmod +x bin/Linux/Release/libsandbox_override.so`
- Check the library path is absolute
- Verify the library was built for the correct architecture

### Windows: "PowerShell script not running"
- Set execution policy: `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass`
- Or use the batch file wrapper: `run_sandbox.bat`

### Windows: "DLL not found"
- Build the sandbox target: `cmake --build build --config Release --target sandbox_override`
- Check that `sandbox_override.dll` exists in `bin/Windows/Release/`
- The script will automatically copy the DLL to the MULO directory

### All Platforms: "sandbox library not found"
- Ensure you've built the project: `cmake --build build`
- Check the `BUILD_TYPE` environment variable matches your build
- Default is `Release`, change to `Debug` if needed

## Environment Variables

### macOS
- `DYLD_INSERT_LIBRARIES`: Set by script to path of `libsandbox_override.dylib`

### Linux
- `LD_PRELOAD`: Set by script to path of `libsandbox_override.so`

### Windows
- `MULO_SANDBOX_ENABLED`: Set to "1" when sandbox is active

### All Platforms
- `BUILD_TYPE`: Override build type (default: `Release`)

**Example:**
```bash
BUILD_TYPE=Debug ./run_with_sandbox.sh
```

## Development

### Adding New Intercepted Functions

1. Edit `src/frontend/sandbox_override.cpp`
2. Add your interception function
3. For macOS: Use `DYLD_INTERPOSE` macro
4. For Linux: Use `dlsym` with `RTLD_NEXT`
5. For Windows: Use IAT hooking or Detours
6. Rebuild: `cmake --build build --target sandbox_override`

### Testing the Sandbox

1. Run MULO with sandbox enabled
2. Load a plugin that attempts restricted operations
3. Check console for `[SANDBOX] BLOCKED` messages
4. Compare behavior with `--no-sandbox` mode

## Security Notes

⚠️ **This is a development tool**, not production-grade security!

- Advanced plugins can bypass userspace interception
- Direct syscalls can bypass libc wrappers
- JIT-compiled code may evade detection
- Kernel-mode code cannot be intercepted

For production security, use OS-level sandboxing:
- **macOS**: App Sandbox with entitlements
- **Linux**: AppArmor, SELinux, seccomp-bpf
- **Windows**: AppContainer, Windows Sandbox

## License

Part of the MULO project. See main LICENSE file.

## More Information

See `SANDBOX_CROSS_PLATFORM.md` for detailed cross-platform implementation notes.
