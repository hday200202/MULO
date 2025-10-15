# Quick Start Guide - Windows Sandbox

## Running the Sandbox Script

You have **TWO easy options** to run the sandbox script on Windows:

### Option 1: Use the Batch File (Easiest! ✅ Recommended)

```batch
.\run_sandbox.bat
```

**Why this works:**
- Automatically bypasses execution policy
- No configuration needed
- Just works!

---

### Option 2: Use PowerShell Script Directly

If you want to run the `.ps1` file directly, you need to bypass the execution policy first.

**One-time setup for current session:**
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\run_with_sandbox.ps1
```

**Or in a single line:**
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force; .\run_with_sandbox.ps1
```

---

### Option 3: Permanent Solution (Optional)

If you want to always allow local scripts to run:

```powershell
Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned
```

Then you can just run:
```powershell
.\run_with_sandbox.ps1
```

---

## Parameters

Both the batch file and PowerShell script support parameters:

### Run WITHOUT sandbox:
```powershell
.\run_with_sandbox.ps1 -NoSandbox
```

### Specify build type:
```powershell
.\run_with_sandbox.ps1 -BuildType Debug
```

### Combined:
```powershell
.\run_with_sandbox.ps1 -BuildType Debug -NoSandbox
```

---

## Building MULO (Complete Build)

MULO requires both the main application AND UI extensions to display properly.

### Easy Way (Recommended):
```powershell
.\build_all.bat
```

This automatically builds:
- MULO executable
- Sandbox DLL
- All UI extension DLLs
- Copies everything to the correct locations

### Manual Build (Advanced):
```powershell
# Build main application
mkdir build
cd build
cmake ..
cmake --build . --config Release
cd ..

# Build UI extensions
cd extensions_toolkit\build
cmake ..
cmake --build . --config Release
cd ..\..

# Copy extensions to output
Copy-Item "extensions_toolkit\build\Release\*.dll" "bin\Windows\Release\extensions\"
```

### Build Only the Sandbox:
```powershell
cmake --build build --config Release --target sandbox_override
```

---

## Troubleshooting

### "Running scripts is disabled on this system"
✅ **Solution:** Use `.\run_sandbox.bat` instead!

Or run:
```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

### "Sandbox library not found"
✅ **Solution:** Build the project first:
```powershell
cmake --build build --config Release --target sandbox_override
```

### "MULO executable not found"
✅ **Solution:** Build the full project:
```powershell
cmake --build build --config Release
```

### "Build directory not found"
✅ **Solution:** Configure CMake:
```powershell
mkdir build
cd build
cmake ..
cd ..
```

---

## Quick Reference Commands

| Task | Command |
|------|---------|
| Run with sandbox | `.\run_sandbox.bat` |
| Run without sandbox | `.\run_with_sandbox.ps1 -NoSandbox` |
| Build sandbox DLL | `cmake --build build --config Release --target sandbox_override` |
| Build everything | `cmake --build build --config Release` |
| Test setup | `.\test_sandbox_setup.ps1` |
| Bypass policy | `Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass` |

---

## File Structure

```
MULO/
├── run_sandbox.bat           ← Use this! (easiest)
├── run_with_sandbox.ps1      ← Or this (needs execution policy)
├── test_sandbox_setup.ps1    ← Test your setup
└── bin/
    └── Windows/
        └── Release/
            ├── MULO.exe
            └── sandbox_override.dll
```

---

## Summary

**Recommended workflow:**

1. **Build everything** (first time):
   ```powershell
   .\build_all.bat
   ```
   This builds MULO + sandbox + all UI extensions

2. **Run with sandbox**:
   ```powershell
   .\run_sandbox.bat
   ```

That's it! 🎉

**Note:** Without the UI extensions, MULO will show a blank screen!

---

**For more details, see:**
- `SANDBOX_CROSS_PLATFORM.md` - Full platform guide
- `SANDBOX_SCRIPTS_README.md` - Script documentation
- `WINDOWS_SANDBOX_IMPLEMENTATION.md` - Technical details
