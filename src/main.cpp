#include "frontend/Application.hpp"
#include <juce_gui_basics/juce_gui_basics.h>

int main() {
    // Initialize JUCE message manager (required on Windows for file dialogs)
    juce::MessageManager::getInstance();
    
    // On Windows, we need to properly initialize JUCE
    #ifdef _WIN32
    juce::initialiseJuce_GUI();
    #endif
    
    Application app;

    while (app.isRunning()) {
        app.update();
        app.render();
    }
    
    #ifdef _WIN32
    juce::shutdownJuce_GUI();
    #endif
    
    return 0;
}