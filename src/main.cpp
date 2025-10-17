#include "frontend/Application.hpp"
#include "audio/Effect.hpp"
#include "DebugConfig.hpp"
#include <juce_events/juce_events.h>

int main() {
    // Ensure JUCE knows this is a standalone app by providing the factory
    // used by JUCE's isStandaloneApp() checks. The Application class
    // derives from juce::JUCEApplication, so provide a createInstance
    // function that returns a new Application.
    juce::JUCEApplicationBase::createInstance = []() -> juce::JUCEApplicationBase* {
        return new Application();
    };

    // Create the scoped JUCE initialiser (this sets up MessageManager and GUI subsystems)
    juce::ScopedJuceInitialiser_GUI libraryInitialiser;

    // Create the application instance via the factory and initialise it
    juce::JUCEApplicationBase* app = juce::JUCEApplicationBase::createInstance();
    app->initialise(juce::String());

    sf::Clock cleanupTimer;

    cleanupTimer.restart();
    // Run the app loop. Application::isRunning() is a member on our concrete
    // Application class, so cast the base pointer to call it.
    auto* concreteApp = dynamic_cast<Application*>(app);
    while (concreteApp && concreteApp->isRunning()) {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        concreteApp->update();
        concreteApp->render();
        
        if (cleanupTimer.getElapsedTime().asMilliseconds() >= 3000) {
            Effect::cleanupScheduledPlugins();
            cleanupTimer.restart();
        }
    }

    Effect::cleanupScheduledPlugins();

    if (concreteApp) {
        concreteApp->shutdown();
    }

    // delete the application instance created via the factory
    delete app;

    // Explicitly delete the MessageManager singleton (ScopedJuceInitialiser_GUI
    // may not destroy it automatically in this manual flow).
    juce::MessageManager::deleteInstance();
    
    return 0;
}