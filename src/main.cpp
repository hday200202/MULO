#include <iostream>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "frontend/ui.hpp"

int main() {
    juce::String s = "JUCE is working!";
    std::cout << s << std::endl;
    loop();
    return 0;
}