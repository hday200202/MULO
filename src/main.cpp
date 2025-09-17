#include "frontend/Application.hpp"
#include "audio/Effect.hpp"
#include "DebugConfig.hpp"
#include <juce_events/juce_events.h>

int main() {
    juce::MessageManager::getInstance();
    
    Application app;
    app.initialise(juce::String());

    sf::Clock cleanupTimer;

    cleanupTimer.restart();
    while (app.isRunning()) {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        app.update();
        app.render();
        
        if (cleanupTimer.getElapsedTime().asMilliseconds() >= 3000) {
            Effect::cleanupScheduledPlugins();
            cleanupTimer.restart();
        }
    }

    Effect::cleanupScheduledPlugins();
    app.shutdown();
    juce::MessageManager::deleteInstance();
    
    return 0;
}