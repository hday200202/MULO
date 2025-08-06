#include "frontend/Application.hpp"
#include <juce_events/juce_events.h>

int main() {
    juce::MessageManager::getInstance();
    
    Application app;
    app.initialise(juce::String());

    while (app.isRunning()) {
        // Process JUCE messages in smaller chunks to avoid blocking SFML
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        app.update();
        app.render();
    }

    app.shutdown();
    juce::MessageManager::deleteInstance();
    
    return 0;
}