@echo off
REM Build MULO and all extensions for Windows

echo [BUILD] Starting MULO Windows Build...
powershell.exe -ExecutionPolicy Bypass -File "%~dp0build_all_windows.ps1" %*

if %ERRORLEVEL% EQU 0 (
    echo.
    echo [SUCCESS] Build completed successfully!
    echo Run MULO with: run_sandbox.bat
) else (
    echo.
    echo [ERROR] Build failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)
