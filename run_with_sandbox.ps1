# Sandbox Test Script for MDAW (Windows PowerShell)
# This script demonstrates how to run MULO with the plugin sandbox enabled

param(
    [switch]$NoSandbox,
    [string]$BuildType = "Release"
)

# Get the directory where this script is located
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = $ScriptDir

Write-Host "[WINDOWS] Detected Windows platform" -ForegroundColor Cyan

# Note: Windows sandbox implementation
$Platform = "Windows"
$SandboxDll = "sandbox_override.dll"

# Find the sandbox library
$SandboxPath = Join-Path $ProjectDir "bin\$Platform\$BuildType\$SandboxDll"

if (-not (Test-Path $SandboxPath)) {
    Write-Host "[ERROR] Sandbox library not found at: $SandboxPath" -ForegroundColor Red
    Write-Host "Please build the project first:" -ForegroundColor Yellow
    Write-Host "  cmake --build build --config $BuildType --target sandbox_override" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Note: Windows sandbox support requires building the sandbox_override DLL" -ForegroundColor Yellow
    exit 1
}

Write-Host "[OK] Found sandbox library: $SandboxPath" -ForegroundColor Green

# Find the MULO executable
$MuloPath = Join-Path $ProjectDir "bin\$Platform\$BuildType\MULO.exe"

if (-not (Test-Path $MuloPath)) {
    Write-Host "[ERROR] MULO executable not found at: $MuloPath" -ForegroundColor Red
    Write-Host "Please build the project first:" -ForegroundColor Yellow
    Write-Host "  cmake --build build --config $BuildType" -ForegroundColor Yellow
    exit 1
}

Write-Host "[OK] Found MULO executable: $MuloPath" -ForegroundColor Green

# Display information
Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  MDAW Plugin Sandbox Test" -ForegroundColor White
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Platform:        $Platform" -ForegroundColor White
Write-Host "Build Type:      $BuildType" -ForegroundColor White
Write-Host "Sandbox Library: $SandboxDll" -ForegroundColor White
Write-Host ""
Write-Host "The sandbox will intercept and block the following operations" -ForegroundColor Yellow
Write-Host "when called by sandboxed plugins:" -ForegroundColor Yellow
Write-Host ""
Write-Host "  [FILE] File System: CreateFile, DeleteFile, CreateDirectory, RemoveDirectory" -ForegroundColor White
Write-Host "  [NET]  Network:     socket, connect, bind, listen, accept, send, recv" -ForegroundColor White
Write-Host "  [EXEC] Execution:   CreateProcess, ShellExecute, system" -ForegroundColor White
Write-Host ""
Write-Host "Monitor the console output for [SANDBOX] BLOCKED messages" -ForegroundColor Yellow
Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

# Option to run with or without sandbox
if ($NoSandbox) {
    Write-Host "[NO SANDBOX] Running MULO WITHOUT sandbox (normal mode)" -ForegroundColor Yellow
    Write-Host ""
    & $MuloPath
} else {
    Write-Host "[SANDBOX] Running MULO WITH sandbox enabled" -ForegroundColor Green
    Write-Host ""
    Write-Host "To run without sandbox, use: .\run_with_sandbox.ps1 -NoSandbox" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Note: Windows sandbox uses DLL injection or detours." -ForegroundColor Yellow
    Write-Host "The sandbox_override.dll should be in the same directory as MULO.exe" -ForegroundColor Yellow
    Write-Host "or you may need to copy it there." -ForegroundColor Yellow
    Write-Host ""
    
    # Check if sandbox DLL is in the same directory as MULO
    $MuloDir = Split-Path -Parent $MuloPath
    $SandboxInMuloDir = Join-Path $MuloDir $SandboxDll
    
    if (-not (Test-Path $SandboxInMuloDir)) {
        Write-Host "[INFO] Copying sandbox DLL to MULO directory..." -ForegroundColor Yellow
        Copy-Item $SandboxPath $SandboxInMuloDir -Force
        Write-Host "[OK] Sandbox DLL copied successfully" -ForegroundColor Green
    }
    
    # Set environment variable for sandbox (if your implementation uses it)
    $env:MULO_SANDBOX_ENABLED = "1"
    
    # Run MULO
    & $MuloPath
    
    # Clean up environment variable
    Remove-Item Env:\MULO_SANDBOX_ENABLED -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "MULO has exited." -ForegroundColor Cyan
