#include "frontend/Application.hpp"
#include <juce_gui_basics/juce_gui_basics.h>

int main() {
    juce::MessageManager::getInstance();
    juce::initialiseJuce_GUI();
    
    Application app;

    while (app.isRunning()) {
        app.update();
        app.render();
    }
    
    juce::shutdownJuce_GUI();
    juce::MessageManager::deleteInstance();

    return 0;
}