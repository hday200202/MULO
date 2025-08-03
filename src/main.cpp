#include "frontend/Application.hpp"
#include <juce_events/juce_events.h>

int main() {
    // Initialize JUCE MessageManager for GUI components (including VST editors)
    juce::MessageManager::getInstance();
    
    Application app;

    while (app.isRunning()) {
        // Process JUCE messages for VST plugin windows - essential for responsiveness
        juce::MessageManager::getInstance()->runDispatchLoopUntil(1);
        
        app.update();
        app.render();
    }

    // Clean up MessageManager
    juce::MessageManager::deleteInstance();
    
    return 0;
}