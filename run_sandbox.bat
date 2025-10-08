@echo off
REM Cross-platform launcher for MULO sandbox
REM This batch file launches the appropriate PowerShell script for Windows

echo Starting MULO with sandbox...
echo.

REM Check if PowerShell is available
where powershell >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: PowerShell is not available on this system.
    echo Please run the PowerShell script directly:
    echo   powershell -ExecutionPolicy Bypass -File run_with_sandbox.ps1
    exit /b 1
)

REM Get the directory where this script is located
set SCRIPT_DIR=%~dp0

REM Run the PowerShell script with execution policy bypass
powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%run_with_sandbox.ps1" %*

exit /b %errorlevel%
