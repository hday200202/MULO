#!/bin/bash

# Test script to verify sandbox setup on all platforms
# Run this to check if your sandbox is properly configured

echo "╔════════════════════════════════════════════════════════════╗"
echo "║  MULO Sandbox Configuration Test                          ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Track test results
PASSED=0
FAILED=0
WARNINGS=0

# Detect platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="Darwin"
    SANDBOX_LIB="libsandbox_override.dylib"
    MULO_EXE="MULO"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    SANDBOX_LIB="libsandbox_override.so"
    MULO_EXE="MULO"
else
    echo -e "${YELLOW}⚠️  Running on Windows or unsupported platform${NC}"
    echo "Please use the PowerShell test script:"
    echo "  .\test_sandbox_setup.ps1"
    exit 1
fi

echo "Platform: $PLATFORM"
echo ""

# Test 1: Check if bash script exists
echo -n "Checking bash script... "
if [ -f "run_with_sandbox.sh" ]; then
    echo -e "${GREEN}✓ Found${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Missing${NC}"
    ((FAILED++))
fi

# Test 2: Check if bash script is executable
echo -n "Checking script permissions... "
if [ -x "run_with_sandbox.sh" ]; then
    echo -e "${GREEN}✓ Executable${NC}"
    ((PASSED++))
else
    echo -e "${YELLOW}⚠ Not executable (run: chmod +x run_with_sandbox.sh)${NC}"
    ((WARNINGS++))
fi

# Test 3: Check for sandbox library in Release
echo -n "Checking sandbox library (Release)... "
if [ -f "bin/$PLATFORM/Release/$SANDBOX_LIB" ]; then
    echo -e "${GREEN}✓ Found${NC}"
    ((PASSED++))
else
    echo -e "${YELLOW}⚠ Not found (build needed)${NC}"
    ((WARNINGS++))
fi

# Test 4: Check for sandbox library in Debug
echo -n "Checking sandbox library (Debug)... "
if [ -f "bin/$PLATFORM/Debug/$SANDBOX_LIB" ]; then
    echo -e "${GREEN}✓ Found${NC}"
    ((PASSED++))
else
    echo -e "${YELLOW}⚠ Not found (optional)${NC}"
    ((WARNINGS++))
fi

# Test 5: Check for MULO executable in Release
echo -n "Checking MULO executable (Release)... "
if [ -f "bin/$PLATFORM/Release/$MULO_EXE" ]; then
    echo -e "${GREEN}✓ Found${NC}"
    ((PASSED++))
else
    echo -e "${YELLOW}⚠ Not found (build needed)${NC}"
    ((WARNINGS++))
fi

# Test 6: Check for MULO executable in Debug
echo -n "Checking MULO executable (Debug)... "
if [ -f "bin/$PLATFORM/Debug/$MULO_EXE" ]; then
    echo -e "${GREEN}✓ Found${NC}"
    ((PASSED++))
else
    echo -e "${YELLOW}⚠ Not found (optional)${NC}"
    ((WARNINGS++))
fi

# Test 7: Check for build directory
echo -n "Checking build directory... "
if [ -d "build" ]; then
    echo -e "${GREEN}✓ Found${NC}"
    ((PASSED++))
else
    echo -e "${RED}✗ Missing (run: mkdir build && cd build && cmake ..)${NC}"
    ((FAILED++))
fi

# Test 8: Check for CMakeLists.txt
echo -n "Checking CMakeLists.txt... "
if [ -f "CMakeLists.txt" ]; then
    echo -e "${GREEN}✓ Found${NC}"
    ((PASSED++))
    
    # Check if sandbox target is in CMakeLists
    if grep -q "sandbox_override" CMakeLists.txt; then
        echo -n "  └─ Sandbox target configured... "
        echo -e "${GREEN}✓${NC}"
    else
        echo -n "  └─ Sandbox target configured... "
        echo -e "${RED}✗${NC}"
        ((FAILED++))
    fi
else
    echo -e "${RED}✗ Missing${NC}"
    ((FAILED++))
fi

# Test 9: Check for documentation
echo -n "Checking documentation... "
DOC_COUNT=0
[ -f "SANDBOX_CROSS_PLATFORM.md" ] && ((DOC_COUNT++))
[ -f "SANDBOX_SCRIPTS_README.md" ] && ((DOC_COUNT++))
[ -f "WINDOWS_SANDBOX_IMPLEMENTATION.md" ] && ((DOC_COUNT++))

if [ $DOC_COUNT -eq 3 ]; then
    echo -e "${GREEN}✓ All docs found${NC}"
    ((PASSED++))
elif [ $DOC_COUNT -gt 0 ]; then
    echo -e "${YELLOW}⚠ Some docs found ($DOC_COUNT/3)${NC}"
    ((WARNINGS++))
else
    echo -e "${YELLOW}⚠ No docs found${NC}"
    ((WARNINGS++))
fi

# Summary
echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║  Test Results                                              ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${YELLOW}Warnings: $WARNINGS${NC}"
echo -e "${RED}Failed: $FAILED${NC}"
echo ""

# Recommendations
if [ $FAILED -gt 0 ] || [ $WARNINGS -gt 0 ]; then
    echo "Recommendations:"
    echo ""
    
    if [ ! -d "build" ] || [ ! -f "bin/$PLATFORM/Release/$MULO_EXE" ]; then
        echo "• Build the project:"
        echo "    mkdir -p build && cd build"
        echo "    cmake .."
        echo "    cmake --build ."
        echo ""
    fi
    
    if [ ! -f "bin/$PLATFORM/Release/$SANDBOX_LIB" ]; then
        echo "• Build the sandbox library:"
        echo "    cmake --build build --target sandbox_override"
        echo ""
    fi
    
    if [ ! -x "run_with_sandbox.sh" ]; then
        echo "• Make script executable:"
        echo "    chmod +x run_with_sandbox.sh"
        echo ""
    fi
else
    echo -e "${GREEN}✓ All checks passed! Your sandbox is ready to use.${NC}"
    echo ""
    echo "To run MULO with sandbox:"
    echo "    ./run_with_sandbox.sh"
    echo ""
    echo "To run without sandbox:"
    echo "    ./run_with_sandbox.sh --no-sandbox"
fi

exit $FAILED
