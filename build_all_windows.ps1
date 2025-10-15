# Build Script for MULO on Windows
# This script builds both the main MULO application and all UI extensions

param(
    [string]$BuildType = "Release"
)

Write-Host "[BUILD] Starting MULO Windows Build..." -ForegroundColor Cyan
Write-Host "[BUILD] Build Type: $BuildType" -ForegroundColor Yellow

$ErrorActionPreference = "Stop"
$MuloRoot = $PSScriptRoot

# Step 1: Build Main MULO Application
Write-Host "`n[STEP 1/3] Building main MULO application..." -ForegroundColor Green

if (-not (Test-Path "$MuloRoot\build")) {
    Write-Host "[INFO] Creating build directory..." -ForegroundColor Gray
    New-Item -ItemType Directory -Path "$MuloRoot\build" | Out-Null
}

Set-Location "$MuloRoot\build"

Write-Host "[CMAKE] Configuring CMake..." -ForegroundColor Gray
cmake .. 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[BUILD] Building MULO executable and sandbox DLL..." -ForegroundColor Gray
cmake --build . --config $BuildType 2>&1 | ForEach-Object {
    if ($_ -match "error|failed") {
        Write-Host $_ -ForegroundColor Red
    }
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] MULO build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[OK] MULO application built successfully!" -ForegroundColor Green

# Step 2: Build UI Extensions
Write-Host "`n[STEP 2/3] Building UI extensions..." -ForegroundColor Green

$ExtensionsPath = "$MuloRoot\extensions_toolkit"
if (-not (Test-Path "$ExtensionsPath\build")) {
    Write-Host "[INFO] Creating extensions build directory..." -ForegroundColor Gray
    New-Item -ItemType Directory -Path "$ExtensionsPath\build" | Out-Null
}

Set-Location "$ExtensionsPath\build"

Write-Host "[CMAKE] Configuring extensions..." -ForegroundColor Gray
cmake .. 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Extensions CMake configuration failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[BUILD] Building extension DLLs..." -ForegroundColor Gray
cmake --build . --config $BuildType 2>&1 | ForEach-Object {
    if ($_ -match "error|failed") {
        Write-Host $_ -ForegroundColor Red
    }
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Extensions build failed!" -ForegroundColor Red
    exit 1
}

Write-Host "[OK] Extensions built successfully!" -ForegroundColor Green

# Step 3: Copy Extensions to MULO Output Directory
Write-Host "`n[STEP 3/3] Copying extensions to MULO..." -ForegroundColor Green

$OutputPath = "$MuloRoot\bin\Windows\$BuildType"
$ExtensionsOutput = "$OutputPath\extensions"

if (-not (Test-Path $ExtensionsOutput)) {
    Write-Host "[INFO] Creating extensions directory in output..." -ForegroundColor Gray
    New-Item -ItemType Directory -Force -Path $ExtensionsOutput | Out-Null
}

Write-Host "[COPY] Copying extension DLLs..." -ForegroundColor Gray
Copy-Item "$ExtensionsPath\build\$BuildType\*.dll" $ExtensionsOutput -Force

$ExtensionCount = (Get-ChildItem $ExtensionsOutput -Filter "*.dll").Count
Write-Host "[OK] Copied $ExtensionCount extension DLLs!" -ForegroundColor Green

# Summary
Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "BUILD COMPLETE!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "MULO Executable: $OutputPath\MULO.exe" -ForegroundColor White
Write-Host "Sandbox DLL:     $OutputPath\sandbox_override.dll" -ForegroundColor White
Write-Host "Extensions:      $ExtensionsOutput ($ExtensionCount DLLs)" -ForegroundColor White
Write-Host "`nRun with: .\run_sandbox.bat" -ForegroundColor Yellow
Write-Host "========================================`n" -ForegroundColor Cyan

Set-Location $MuloRoot
