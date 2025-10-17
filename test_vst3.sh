#!/bin/bash

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  VST3 Plugin Detection Test"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Check if Vital exists
VST3_PATH="/Library/Audio/Plug-Ins/VST3/Vital.vst3"
if [ -d "$VST3_PATH" ]; then
    echo "✅ Vital.vst3 found at: $VST3_PATH"
else
    echo "❌ Vital.vst3 not found at: $VST3_PATH"
    exit 1
fi

# Check bundle structure
echo ""
echo "📦 VST3 Bundle Structure:"
ls -la "$VST3_PATH"

echo ""
echo "📦 Contents:"
ls -la "$VST3_PATH/Contents/"

echo ""
echo "📦 MacOS Binary:"
ls -la "$VST3_PATH/Contents/MacOS/"

# Check the binary
BINARY="$VST3_PATH/Contents/MacOS/Vital"
if [ -f "$BINARY" ]; then
    echo ""
    echo "✅ Binary found: $BINARY"
    echo "   File type:"
    file "$BINARY"
    echo ""
    echo "   Architecture:"
    lipo -info "$BINARY" 2>/dev/null || echo "   (lipo command not available)"
    echo ""
    echo "   Code signature:"
    codesign -dv "$BINARY" 2>&1 | grep -E "Signature|Identifier|Authority" || echo "   Not signed or unable to verify"
else
    echo "❌ Binary not found at: $BINARY"
fi

# Check if JUCE can see it
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  JUCE Plugin Scanner Test"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Try to run MULO and capture VST-related output
cd /Users/prashantneupane/Desktop/test_mdaw/MDAW
echo "Attempting to scan for VST3 plugins..."
timeout 5 ./bin/Darwin/Release/MULO 2>&1 | grep -i "vital\|vst3\|plugin" | head -30

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Recommendations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "If Vital is not loading:"
echo "  1. Make sure JUCE VST3 support is enabled in CMakeLists.txt"
echo "  2. Check JUCE_PLUGINHOST_VST3=1 is defined"
echo "  3. Verify the binary architecture matches your system"
echo "  4. Try re-installing Vital VST3"
echo "  5. Check console output for more detailed JUCE errors"
echo ""
