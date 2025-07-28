#include "frontend/Application.hpp"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

int main() {
    juce::ScopedJuceInitialiser_GUI juceInitializer;
    
    Application app;

    while (app.isRunning()) {
        app.update();
        app.render();
    }

    return 0;
}