# Test script to verify sandbox setup on Windows
# Run this to check if your sandbox is properly configured

param(
    [string]$BuildType = "Release"
)

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  MULO Sandbox Configuration Test (Windows)                ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

# Track test results
$Passed = 0
$Failed = 0
$Warnings = 0

$Platform = "Windows"
$SandboxDll = "sandbox_override.dll"
$MuloExe = "MULO.exe"

Write-Host "Platform: $Platform"
Write-Host "Build Type: $BuildType"
Write-Host ""

# Test 1: Check if PowerShell script exists
Write-Host -NoNewline "Checking PowerShell script... "
if (Test-Path "run_with_sandbox.ps1") {
    Write-Host "✓ Found" -ForegroundColor Green
    $Passed++
} else {
    Write-Host "✗ Missing" -ForegroundColor Red
    $Failed++
}

# Test 2: Check if batch script exists
Write-Host -NoNewline "Checking batch launcher... "
if (Test-Path "run_sandbox.bat") {
    Write-Host "✓ Found" -ForegroundColor Green
    $Passed++
} else {
    Write-Host "⚠ Missing (optional)" -ForegroundColor Yellow
    $Warnings++
}

# Test 3: Check for sandbox DLL in Release
Write-Host -NoNewline "Checking sandbox DLL ($BuildType)... "
$SandboxPath = "bin\$Platform\$BuildType\$SandboxDll"
if (Test-Path $SandboxPath) {
    Write-Host "✓ Found" -ForegroundColor Green
    $Passed++
    
    # Check DLL properties
    $DllInfo = Get-Item $SandboxPath
    Write-Host "  └─ Size: $($DllInfo.Length) bytes" -ForegroundColor Gray
    Write-Host "  └─ Modified: $($DllInfo.LastWriteTime)" -ForegroundColor Gray
} else {
    Write-Host "⚠ Not found (build needed)" -ForegroundColor Yellow
    $Warnings++
}

# Test 4: Check for sandbox DLL in Debug (if checking Release)
if ($BuildType -eq "Release") {
    Write-Host -NoNewline "Checking sandbox DLL (Debug)... "
    if (Test-Path "bin\$Platform\Debug\$SandboxDll") {
        Write-Host "✓ Found" -ForegroundColor Green
        $Passed++
    } else {
        Write-Host "⚠ Not found (optional)" -ForegroundColor Yellow
        $Warnings++
    }
}

# Test 5: Check for MULO executable
Write-Host -NoNewline "Checking MULO executable ($BuildType)... "
$MuloPath = "bin\$Platform\$BuildType\$MuloExe"
if (Test-Path $MuloPath) {
    Write-Host "✓ Found" -ForegroundColor Green
    $Passed++
    
    $ExeInfo = Get-Item $MuloPath
    Write-Host "  └─ Size: $($ExeInfo.Length) bytes" -ForegroundColor Gray
} else {
    Write-Host "⚠ Not found (build needed)" -ForegroundColor Yellow
    $Warnings++
}

# Test 6: Check for MULO executable in Debug (if checking Release)
if ($BuildType -eq "Release") {
    Write-Host -NoNewline "Checking MULO executable (Debug)... "
    if (Test-Path "bin\$Platform\Debug\$MuloExe") {
        Write-Host "✓ Found" -ForegroundColor Green
        $Passed++
    } else {
        Write-Host "⚠ Not found (optional)" -ForegroundColor Yellow
        $Warnings++
    }
}

# Test 7: Check for build directory
Write-Host -NoNewline "Checking build directory... "
if (Test-Path "build") {
    Write-Host "✓ Found" -ForegroundColor Green
    $Passed++
    
    # Check for Visual Studio solution or CMake cache
    if (Test-Path "build\CMakeCache.txt") {
        Write-Host "  └─ CMake configured... " -NoNewline -ForegroundColor Gray
        Write-Host "✓" -ForegroundColor Green
    } else {
        Write-Host "  └─ CMake configured... " -NoNewline -ForegroundColor Gray
        Write-Host "✗" -ForegroundColor Red
        $Failed++
    }
} else {
    Write-Host "✗ Missing" -ForegroundColor Red
    $Failed++
}

# Test 8: Check for CMakeLists.txt
Write-Host -NoNewline "Checking CMakeLists.txt... "
if (Test-Path "CMakeLists.txt") {
    Write-Host "✓ Found" -ForegroundColor Green
    $Passed++
    
    # Check if sandbox target is in CMakeLists
    $CMakeContent = Get-Content "CMakeLists.txt" -Raw
    if ($CMakeContent -match "sandbox_override") {
        Write-Host "  └─ Sandbox target configured... " -NoNewline -ForegroundColor Gray
        Write-Host "✓" -ForegroundColor Green
        
        # Check for Windows-specific configuration
        if ($CMakeContent -match "WIN32" -and $CMakeContent -match "sandbox_override") {
            Write-Host "  └─ Windows sandbox support... " -NoNewline -ForegroundColor Gray
            Write-Host "✓" -ForegroundColor Green
        } else {
            Write-Host "  └─ Windows sandbox support... " -NoNewline -ForegroundColor Gray
            Write-Host "⚠" -ForegroundColor Yellow
            $Warnings++
        }
    } else {
        Write-Host "  └─ Sandbox target configured... " -NoNewline -ForegroundColor Gray
        Write-Host "✗" -ForegroundColor Red
        $Failed++
    }
} else {
    Write-Host "✗ Missing" -ForegroundColor Red
    $Failed++
}

# Test 9: Check for documentation
Write-Host -NoNewline "Checking documentation... "
$DocCount = 0
if (Test-Path "SANDBOX_CROSS_PLATFORM.md") { $DocCount++ }
if (Test-Path "SANDBOX_SCRIPTS_README.md") { $DocCount++ }
if (Test-Path "WINDOWS_SANDBOX_IMPLEMENTATION.md") { $DocCount++ }

if ($DocCount -eq 3) {
    Write-Host "✓ All docs found" -ForegroundColor Green
    $Passed++
} elseif ($DocCount -gt 0) {
    Write-Host "⚠ Some docs found ($DocCount/3)" -ForegroundColor Yellow
    $Warnings++
} else {
    Write-Host "⚠ No docs found" -ForegroundColor Yellow
    $Warnings++
}

# Test 10: Check PowerShell version
Write-Host -NoNewline "Checking PowerShell version... "
$PSVer = $PSVersionTable.PSVersion
if ($PSVer.Major -ge 5) {
    Write-Host "✓ $PSVer" -ForegroundColor Green
    $Passed++
} else {
    Write-Host "⚠ $PSVer (5.0+ recommended)" -ForegroundColor Yellow
    $Warnings++
}

# Test 11: Check Visual C++ Redistributable (common dependency)
Write-Host -NoNewline "Checking for MSVC runtime... "
$VCRedist = Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*" -ErrorAction SilentlyContinue |
            Where-Object { $_.DisplayName -like "*Visual C++ * Redistributable*" }

if ($VCRedist) {
    Write-Host "✓ Found" -ForegroundColor Green
    $Passed++
} else {
    Write-Host "⚠ Not detected (may not be needed)" -ForegroundColor Yellow
    $Warnings++
}

# Summary
Write-Host ""
Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Test Results                                              ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host "Passed: $Passed" -ForegroundColor Green
Write-Host "Warnings: $Warnings" -ForegroundColor Yellow
Write-Host "Failed: $Failed" -ForegroundColor Red
Write-Host ""

# Recommendations
if ($Failed -gt 0 -or $Warnings -gt 0) {
    Write-Host "Recommendations:" -ForegroundColor Cyan
    Write-Host ""
    
    if (-not (Test-Path "build") -or -not (Test-Path $MuloPath)) {
        Write-Host "• Build the project:" -ForegroundColor Yellow
        Write-Host "    mkdir build; cd build" -ForegroundColor White
        Write-Host "    cmake .." -ForegroundColor White
        Write-Host "    cmake --build . --config $BuildType" -ForegroundColor White
        Write-Host ""
    }
    
    if (-not (Test-Path $SandboxPath)) {
        Write-Host "• Build the sandbox library:" -ForegroundColor Yellow
        Write-Host "    cmake --build build --config $BuildType --target sandbox_override" -ForegroundColor White
        Write-Host ""
    }
    
    if ($Warnings -gt 2) {
        Write-Host "• Some optional components are missing, but they won't prevent basic operation." -ForegroundColor Yellow
        Write-Host ""
    }
} else {
    Write-Host "✓ All checks passed! Your sandbox is ready to use." -ForegroundColor Green
    Write-Host ""
    Write-Host "To run MULO with sandbox:" -ForegroundColor Cyan
    Write-Host "    .\run_with_sandbox.ps1" -ForegroundColor White
    Write-Host ""
    Write-Host "To run without sandbox:" -ForegroundColor Cyan
    Write-Host "    .\run_with_sandbox.ps1 -NoSandbox" -ForegroundColor White
    Write-Host ""
    Write-Host "Using batch file:" -ForegroundColor Cyan
    Write-Host "    run_sandbox.bat" -ForegroundColor White
}

Write-Host ""
Write-Host "For more information, see:" -ForegroundColor Gray
Write-Host "  - SANDBOX_CROSS_PLATFORM.md" -ForegroundColor Gray
Write-Host "  - SANDBOX_SCRIPTS_README.md" -ForegroundColor Gray
Write-Host "  - WINDOWS_SANDBOX_IMPLEMENTATION.md" -ForegroundColor Gray

exit $Failed
