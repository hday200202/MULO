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

    virtual void init() = 0;
    virtual void update() = 0;
    virtual Container* getLayout() { return layout; }
    virtual bool handleEvents() = 0;

    // Visibility control
    virtual void show() { if (layout) layout->m_modifier.setVisible(true); }
    virtual void hide() { if (layout) layout->m_modifier.setVisible(false); }
    virtual bool isVisible() const { return layout ? layout->m_modifier.isVisible() : false; }
    virtual void setVisible(bool visible) { if (visible) show(); else hide(); }
    virtual void toggle() { if (isVisible()) hide(); else show(); }

    // Set references to Application, Engine, UIState, and UIResources
    inline void setEngineRef(Engine* engineRef) { engine = engineRef; }
    inline void setAppRef(Application* appRef) { app = appRef; }
    inline void setUIStateRef(UIState* uiStateRef) { uiState = uiStateRef; }
    inline void setResourcesRef(UIResources* resourcesRef) { resources = resourcesRef; }
    
    // Set parent container reference for layout hierarchy
    inline void setParentContainer(Container* parent) { parentContainer = parent; }

    inline std::string getName() const { return name; }
    inline bool isInitialized() const { return initialized; }

protected:
    Engine* engine = nullptr;
    Application* app = nullptr;
    UIState* uiState = nullptr;
    UIResources* resources = nullptr;

    Container* layout = nullptr;
    Container* parentContainer = nullptr;

    std::string name = "";
    bool initialized = false;
    bool forceUpdate = false;
};