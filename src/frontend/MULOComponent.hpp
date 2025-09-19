
#pragma once

#include "UILO/UILO.hpp"
#include "../DebugConfig.hpp"
// #include <juce_gui_basics/juce_gui_basics.h>

class Application;
class MIDIClip;  // Forward declaration

using namespace uilo;

// Plugin interface for dynamic loading
extern "C" {
    typedef struct {
        void* instance;
        void (*init)(void* instance, Application* app);
        void (*update)(void* instance);
        bool (*handleEvents)(void* instance);
        bool (*isInitialized)(void* instance);
        void (*destroy)(void* instance);
        const char* (*getName)(void* instance);
        void (*show)(void* instance);
        void (*hide)(void* instance);
        bool (*isVisible)(void* instance);
        void (*setVisible)(void* instance, bool visible);
        void (*toggle)(void* instance);
        void* (*getLayout)(void* instance);
        void* (*getParentContainer)(void* instance);
        void (*setParentContainer)(void* instance, Container* parent);
        MIDIClip* (*getSelectedMIDIClip)(void* instance);  // Add MIDI clip selection method
    } PluginVTable;
    
    typedef PluginVTable* (*CreatePluginFunc)();
}

class MULOComponent {
public:
    std::string relativeTo = "";

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
    inline void setAppRef(Application* appRef) { app = appRef; }
    virtual inline void setParentContainer(Container* parent) { parentContainer = parent; }
    virtual Container* getParentContainer() const { return parentContainer; }
    inline std::string getName() const { return name; }
    virtual inline std::string getRelativeTo() const { return relativeTo; }
    virtual inline void setRelativeTo(const std::string& relative) { relativeTo = relative; }
    virtual bool isInitialized() const { return initialized; }
    
    virtual MIDIClip* getSelectedMIDIClip() const { 
        DEBUG_PRINT("[BASE] MULOComponent::getSelectedMIDIClip() called - returning nullptr");
        return nullptr; 
    }

protected:
    Application* app = nullptr;
    Container* layout = nullptr;
    Container* parentContainer = nullptr;

    std::string name = "";
    bool initialized = false;
    bool forceUpdate = false;
};

class PluginComponentWrapper : public MULOComponent {
public:
    explicit PluginComponentWrapper(PluginVTable* pluginVTable) 
        : plugin(pluginVTable) {
        if (plugin && plugin->getName) {
            name = plugin->getName(plugin->instance);
        }
    }
    
    ~PluginComponentWrapper() override {
        DEBUG_PRINT("Destroying PluginComponentWrapper for: " << name << " (" << this << ")");
    }
    
    void init() override {
        if (plugin && plugin->init) {
            plugin->init(plugin->instance, app);
            initialized = true;
        }
    }
    
    void update() override {
        if (plugin && plugin->update) {
            plugin->update(plugin->instance);
        }
    }
    
    bool handleEvents() override {
        if (plugin && plugin->handleEvents) {
            return plugin->handleEvents(plugin->instance);
        }
        return false;
    }
    
    Container* getLayout() override {
        if (plugin && plugin->getLayout) {
            return static_cast<Container*>(plugin->getLayout(plugin->instance));
        }
        return nullptr;
    }
    
    void show() override {
        if (plugin && plugin->show) {
            plugin->show(plugin->instance);
        }
    }
    
    void hide() override {
        if (plugin && plugin->hide) {
            plugin->hide(plugin->instance);
        }
    }
    
    bool isVisible() const override {
        if (plugin && plugin->isVisible) {
            return plugin->isVisible(plugin->instance);
        }
        return false;
    }
    
    void setVisible(bool visible) override {
        if (plugin && plugin->setVisible) {
            plugin->setVisible(plugin->instance, visible);
        }
    }
    
    void toggle() override {
        if (plugin && plugin->toggle) {
            plugin->toggle(plugin->instance);
        }
    }
    
    bool isInitialized() const override {
        if (plugin && plugin->isInitialized) {
            return plugin->isInitialized(plugin->instance);
        }
        return initialized;
    }

    Container* getParentContainer() const override {
        if (plugin && plugin->instance) {
            MULOComponent* pluginInstance = static_cast<MULOComponent*>(plugin->instance);
            return pluginInstance->getParentContainer();
        }
        return parentContainer;
    }

    std::string getRelativeTo() const override {
        if (plugin && plugin->instance) {
            MULOComponent* pluginInstance = static_cast<MULOComponent*>(plugin->instance);
            return pluginInstance->getRelativeTo();
        }
        return relativeTo;
    }

    void setRelativeTo(const std::string& relative) override {
        if (plugin && plugin->instance) {
            MULOComponent* pluginInstance = static_cast<MULOComponent*>(plugin->instance);
            pluginInstance->setRelativeTo(relative);
        } else {
            relativeTo = relative;
        }
    }

    void setParentContainer(Container* parent) override {
        if (plugin && plugin->setParentContainer) {
            plugin->setParentContainer(plugin->instance, parent);
        } else {
            parentContainer = parent;
        }
    }
    
    // Override getSelectedMIDIClip to forward to plugin implementation
    MIDIClip* getSelectedMIDIClip() const override {
        if (plugin && plugin->getSelectedMIDIClip) {
            DEBUG_PRINT("[WRAPPER] Forwarding getSelectedMIDIClip to plugin");
            return plugin->getSelectedMIDIClip(plugin->instance);
        }
        DEBUG_PRINT("[WRAPPER] No getSelectedMIDIClip in plugin, returning base");
        return MULOComponent::getSelectedMIDIClip();
    }

    friend class Application;

protected:
    PluginVTable* plugin = nullptr;
};

#ifdef _WIN32
#define GET_INTERFACE \
    extern "C" { \
        __declspec(dllexport) PluginVTable* getPluginInterface(); \
    }

#else
#define GET_INTERFACE \
    extern "C" { \
        __attribute__((visibility("default"))) PluginVTable* getPluginInterface(); \
    }

#endif

// Helper macros for creating plugins
#define DECLARE_PLUGIN(ClassName) \
    extern "C" { \
        void plugin_init(void* instance, Application* app) { \
            if (instance) { \
                static_cast<ClassName*>(instance)->setAppRef(app); \
                static_cast<ClassName*>(instance)->init(); \
            } \
        } \
        void plugin_update(void* instance) { \
            if (instance) \
                static_cast<ClassName*>(instance)->update(); \
        } \
        bool plugin_handleEvents(void* instance) { \
            if (instance) \
                return static_cast<ClassName*>(instance)->handleEvents(); \
            return false; \
        } \
        bool plugin_isInitialized(void* instance) { \
            if (instance) \
                return static_cast<ClassName*>(instance)->isInitialized(); \
            return false; \
        } \
        void plugin_destroy(void* instance) { \
            if (instance) \
                delete static_cast<ClassName*>(instance); \
        } \
        const char* plugin_getName(void* instance) { \
            if (instance) { \
                static thread_local std::string name = static_cast<ClassName*>(instance)->getName(); \
                return name.c_str(); \
            } \
            return ""; \
        } \
        void plugin_show(void* instance) { \
            if (instance) \
                static_cast<ClassName*>(instance)->show(); \
        } \
        void plugin_hide(void* instance) { \
            if (instance) \
                static_cast<ClassName*>(instance)->hide(); \
        } \
        bool plugin_isVisible(void* instance) { \
            if (instance) \
                return static_cast<ClassName*>(instance)->isVisible(); \
            return false; \
        } \
        void plugin_setVisible(void* instance, bool visible) { \
            if (instance) \
                static_cast<ClassName*>(instance)->setVisible(visible); \
        } \
        void plugin_toggle(void* instance) { \
            if (instance) \
                static_cast<ClassName*>(instance)->toggle(); \
        } \
        void* plugin_getLayout(void* instance) { \
            if (instance) \
                return static_cast<void*>(static_cast<ClassName*>(instance)->getLayout()); \
            return nullptr; \
        } \
        void* plugin_getParentContainer(void* instance) { \
            if (instance) \
                return static_cast<void*>(static_cast<ClassName*>(instance)->getParentContainer()); \
            return nullptr; \
        } \
        void plugin_setParentContainer(void* instance, Container* parent) { \
            if (instance) \
                static_cast<ClassName*>(instance)->setParentContainer(parent); \
        } \
        MIDIClip* plugin_getSelectedMIDIClip(void* instance) { \
            if (instance) \
                return static_cast<ClassName*>(instance)->getSelectedMIDIClip(); \
            return nullptr; \
        } \
        PluginVTable* getPluginInterface() { \
            static PluginVTable vtable = { \
                nullptr, /* instance will be set below */ \
                plugin_init, \
                plugin_update, \
                plugin_handleEvents, \
                plugin_isInitialized, \
                plugin_destroy, \
                plugin_getName, \
                plugin_show, \
                plugin_hide, \
                plugin_isVisible, \
                plugin_setVisible, \
                plugin_toggle, \
                plugin_getLayout, \
                plugin_getParentContainer, \
                plugin_setParentContainer, \
                plugin_getSelectedMIDIClip \
            }; \
            vtable.instance = new ClassName(); \
            return &vtable; \
        } \
    }
