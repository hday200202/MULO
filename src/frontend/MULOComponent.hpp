#pragma once

#include "UILO/UILO.hpp"
#include <juce_gui_basics/juce_gui_basics.h>

class Application;
class Engine;
struct UIState;
struct UIResources;

using namespace uilo;

class MULOComponent {
public:
    MULOComponent() = default;
    virtual ~MULOComponent() = default;

    virtual void update() = 0;
    virtual uilo::Container* getLayout() = 0;

    // Set references to Application, Engine, UIState, and UIResources
    inline void setEngineRef(Engine* engineRef) { engine = engineRef; }
    inline void setAppRef(Application* appRef) { app = appRef; }
    inline void setUIStateRef(UIState* uiStateRef) { uiState = uiStateRef; }
    inline void setResourcesRef(UIResources* resourcesRef) { resources = resourcesRef; }

    // Handle input events and update custom UI elements
    virtual void handleEvents() = 0;
    virtual void handleCustomUIElements() = 0;

    virtual void rebuildUI() = 0;

protected:
    Engine* engine = nullptr;
    Application* app = nullptr;
    UIState* uiState = nullptr;
    UIResources* resources = nullptr;

    uilo::Container* layout = nullptr;
};