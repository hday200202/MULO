#include <iostream>
#include <juce_core/juce_core.h>

#include "frontend/ui.hpp"

int main() {
    juce::String s = "JUCE is working!";
    std::cout << s << std::endl;
    loop();
    return 0;
}