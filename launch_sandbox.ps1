# Universal MULO Sandbox Launcher
# This script detects your platform and runs the appropriate sandbox launcher

Write-Host "MULO Sandbox Launcher" -ForegroundColor Cyan
Write-Host "=====================" -ForegroundColor Cyan
Write-Host ""

# Detect platform
if ($IsWindows -or $env:OS -eq "Windows_NT") {
    Write-Host "Platform: Windows" -ForegroundColor Green
    Write-Host "Launching Windows PowerShell script..." -ForegroundColor Yellow
    Write-Host ""
    
    $scriptPath = Join-Path $PSScriptRoot "run_with_sandbox.ps1"
    & $scriptPath @args
    
} elseif ($IsMacOS) {
    Write-Host "Platform: macOS" -ForegroundColor Green
    Write-Host "Please use the bash script:" -ForegroundColor Yellow
    Write-Host "  ./run_with_sandbox.sh" -ForegroundColor White
    
} elseif ($IsLinux) {
    Write-Host "Platform: Linux" -ForegroundColor Green
    Write-Host "Please use the bash script:" -ForegroundColor Yellow
    Write-Host "  ./run_with_sandbox.sh" -ForegroundColor White
    
} else {
    Write-Host "Platform: Unknown" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please run the appropriate script for your platform:" -ForegroundColor Yellow
    Write-Host "  Windows: .\run_with_sandbox.ps1" -ForegroundColor White
    Write-Host "  macOS:   ./run_with_sandbox.sh" -ForegroundColor White
    Write-Host "  Linux:   ./run_with_sandbox.sh" -ForegroundColor White
}
