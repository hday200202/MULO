#include "Application.hpp"
#include <filesystem>
#include <iostream>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

bool Application::isRunning() const { 
    return running; 
}

Container* Application::getComponentLayout(const std::string& componentName) { 
    if (muloComponents.find(componentName) != muloComponents.end()) 
        return muloComponents[componentName]->getLayout(); 
    return nullptr;
}

MULOComponent* Application::getComponent(const std::string& componentName) {
    auto it = muloComponents.find(componentName);
    return (it != muloComponents.end()) ? it->second.get() : nullptr;
}

Container* Application::getPageBaseContainer() { 
    return baseContainer; 
}

Row* Application::getMainContentRow() { 
    return mainContentRow; 
}

const sf::RenderWindow& Application::getWindow() const { 
    return window; 
}

void Application::requestUIRebuild() { 
    pendingUIRebuild = true; 
}

void Application::requestFullscreenToggle() { 
    pendingFullscreenToggle = true; 
}

void Application::loadComponents() {
    scanAndLoadPlugins();

    for (auto& [name, component] : muloComponents) {
        if (!component) {
             std::cerr << "Error: Null component created for key: " << name << std::endl;
             continue;
        }
        component->setAppRef(this);
    }

    bool allInitialized = false;
    int attempts = 0;
    while (!allInitialized && attempts < 15) {
        allInitialized = true;
        for (auto& [name, component] : muloComponents) {
            if (!component->isInitialized()) {
                component->init();
                allInitialized = false;
            }
        }
        ++attempts;
    }

    for (auto& [name, component] : muloComponents) {
        componentLayouts[name] = { 
            (component->getParentContainer()) ? component->getParentContainer() : nullptr, 
            (component->getLayout()) ? component->getLayout()->m_modifier.getAlignment() : Align::NONE, 
            component->getRelativeTo() 
        };
    }

    DEBUG_PRINT("\nComponent Layout Data: ");
    DEBUG_PRINT("=========================================");
    for (const auto& [name, layoutData] : componentLayouts) {
        DEBUG_PRINT("Component: " << name);
        DEBUG_PRINT("  Parent Container: " << (layoutData.parent ? layoutData.parent->m_name : "NULL"));
        DEBUG_PRINT("  Alignment: " << getAlignmentString(layoutData.alignment));
        DEBUG_PRINT("  Relative To: " << layoutData.relativeTo << "\n");
    }
    DEBUG_PRINT("=========================================\n");

    if (!allInitialized) {
        std::cout << "Couldn't Initialize Components: \n";

        for (auto& [name, component] : muloComponents)
            if (!component->isInitialized())
                std::cout << "\t" + name + "\n";
    }
}

void Application::scanAndLoadPlugins() {
    std::string pluginDir = exeDirectory + "/extensions";
    if (!fs::exists(pluginDir) || !fs::is_directory(pluginDir))
        return;

#ifdef _WIN32
    constexpr const char* pluginExt = ".dll";
#elif __APPLE__
    constexpr const char* pluginExt = ".dylib";
#else
    constexpr const char* pluginExt = ".so";
#endif

    for (const auto& entry : fs::directory_iterator(pluginDir)) {
        if (entry.is_regular_file() && entry.path().extension() == pluginExt) {
            std::string pluginPath = entry.path().string();
            DEBUG_PRINT("Found plugin: " << pluginPath);
            if (loadPlugin(pluginPath)) {
                DEBUG_PRINT("Successfully loaded plugin: " << pluginPath);
            } else {
                DEBUG_PRINT("Failed to load plugin: " << pluginPath);
            }
        }
    }
}

bool Application::loadPlugin(const std::string& pluginPath) {
    try {
        fs::path pluginFile(pluginPath);
        std::string pluginName = pluginFile.filename().string();
        
        bool isTrusted = isPluginTrusted(pluginName);

#ifdef _WIN32
        HMODULE handle = LoadLibraryA(pluginPath.c_str());
        if (!handle) {
            std::cerr << "Failed to load library: " << pluginPath << " (Error: " << GetLastError() << ")" << std::endl;
            return false;
        }

        typedef PluginVTable* (*GetPluginInterfaceFunc)();
        GetPluginInterfaceFunc getPluginInterface = (GetPluginInterfaceFunc)GetProcAddress(handle, "getPluginInterface");
        
        if (!getPluginInterface) {
            std::cerr << "Plugin missing getPluginInterface function: " << pluginPath << std::endl;
            FreeLibrary(handle);
            return false;
        }
#else
        void* handle = dlopen(pluginPath.c_str(), RTLD_LAZY);
        if (!handle) {
            std::cerr << "Failed to load library: " << pluginPath << " (" << dlerror() << ")" << std::endl;
            return false;
        }

        dlerror();

        typedef PluginVTable* (*GetPluginInterfaceFunc)();
        GetPluginInterfaceFunc getPluginInterface = (GetPluginInterfaceFunc)dlsym(handle, "getPluginInterface");
        
        const char* dlsym_error = dlerror();
        if (dlsym_error) {
            std::cerr << "Plugin missing getPluginInterface function: " << pluginPath << " (" << dlsym_error << ")" << std::endl;
            dlclose(handle);
            return false;
        }
#endif

        PluginVTable* vtable = getPluginInterface();
        if (!vtable || !vtable->init || !vtable->getName) {
            std::cerr << "Invalid plugin interface: " << pluginPath << std::endl;
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return false;
        }

        const char* pluginNameCStr = vtable->getName(vtable->instance);
        if (!pluginNameCStr) {
            std::cerr << "Plugin name is null: " << pluginPath << std::endl;
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return false;
        }

        std::string name(pluginNameCStr);
        
        if (muloComponents.find(name) != muloComponents.end()) {
            DEBUG_PRINT("Plugin with name '" << name << "' already loaded, skipping");
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return false;
        }

        auto wrapper = std::make_unique<PluginComponentWrapper>(vtable, !isTrusted, pluginName);
        
        LoadedPlugin loadedPlugin;
        loadedPlugin.path = pluginPath;
        loadedPlugin.handle = handle;
        loadedPlugin.plugin = vtable;
        loadedPlugin.name = name;
        loadedPlugin.isSandboxed = !isTrusted;
        loadedPlugin.isTrusted = isTrusted;
        loadedPlugins[name] = std::move(loadedPlugin);

        muloComponents[name] = std::move(wrapper);
        
        std::cout << "Plugin '" << name << "' loaded successfully";
        if (!isTrusted) {
            std::cout << " (sandboxed)";
        } else {
            std::cout << " (trusted, no sandbox)";
        }
        std::cout << std::endl;
        
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception loading plugin " << pluginPath << ": " << e.what() << std::endl;
        return false;
    }
}

void Application::unloadPlugin(const std::string& pluginName) {
    auto componentIt = muloComponents.find(pluginName);
    if (componentIt != muloComponents.end()) {
        if (auto* wrapper = dynamic_cast<PluginComponentWrapper*>(componentIt->second.get())) {
            if (wrapper->isSandboxed()) {
                wrapper->cleanupSandbox();
                DEBUG_PRINT("Cleaned up sandbox for plugin: " << pluginName);
            }
            wrapper->plugin = nullptr;
        }
        componentIt->second.reset();
        muloComponents.erase(componentIt);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto it = loadedPlugins.find(pluginName);
    if (it != loadedPlugins.end()) {
        if (it->second.plugin && it->second.plugin->destroy) {
            it->second.plugin->destroy(it->second.plugin->instance);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

#ifdef _WIN32
        if (it->second.handle) {
            FreeLibrary((HMODULE)it->second.handle);
        }
#else
        if (it->second.handle) {
            dlclose(it->second.handle);
        }
#endif

        loadedPlugins.erase(it);
        std::cout << "Plugin '" << pluginName << "' unloaded successfully" << std::endl;
    }
}

void Application::unloadAllPlugins() {
    std::vector<std::string> pluginNames;
    for (const auto& plugin : loadedPlugins) {
        pluginNames.push_back(plugin.first);
    }

    for (const auto& name : pluginNames) {
        unloadPlugin(name);
    }

    uilo_owned_elements.clear();
    high_priority_elements.clear();
}

void Application::setPluginTrusted(const std::string& pluginName, bool trusted) {
    // Check database
}

bool Application::isPluginTrusted(const std::string& pluginName) const {
    static const std::unordered_map<std::string, std::string> pluginToExtensionMap = {
        {"TimelineComponent.so", "timeline"},
        {"PianoRollComponent.so", "piano_roll"},
        {"MixerComponent.so", "mixer"},
        {"FXRackComponent.so", "fxrack"},
        {"MarketplaceComponent.so", "marketplace"},
        {"SettingsComponent.so", "settings"},
        {"KBShortcuts.so", "keyboard_shortcuts"},
        {"FileBrowserComponent.so", "filebrowser"},
        {"AppControls.so", "app_controls"}
    };
    
    auto it = pluginToExtensionMap.find(pluginName);
    if (it != pluginToExtensionMap.end()) {
        const std::string& extensionId = it->second;
        
        if (firebaseState == FirebaseState::Success) {
            for (const auto& ext : extensions) {
                if (ext.id == extensionId) {
                    bool verified = ext.verified;
                    std::cout << "Plugin '" << pluginName << "' Firebase verification: " << (verified ? "VERIFIED" : "UNVERIFIED") << std::endl;
                    return verified;
                }
            }
        }
    }
    
    static const std::vector<std::string> fallbackTrustedPlugins = {
        "TimelineComponent.so",
        "PianoRollComponent.so",
        "MixerComponent.so",
        "FXRackComponent.so",
        "MarketplaceComponent.so",
        "AppControls.so",
        "MULOCollab.so",
        "UserLogin.so",
        "FileBrowserComponent.so",
        "ExtensionUploader.so",
        "KBShortcuts.so"
    };
    
    bool isTrusted = std::find(fallbackTrustedPlugins.begin(), fallbackTrustedPlugins.end(), pluginName) != fallbackTrustedPlugins.end();
    std::cout << "Plugin '" << pluginName << "' using fallback trust: " << (isTrusted ? "TRUSTED" : "SANDBOXED") << std::endl;
    return isTrusted;
}

void Application::handleDragAndDrop() {
    using namespace sf::Keyboard;
    using namespace sf::Mouse;
    using mb = sf::Mouse::Button;
    using kb = sf::Keyboard::Key;

    bool alt = isKeyPressed(kb::LAlt) || isKeyPressed(kb::RAlt);
    bool dragging = alt && ui->isMouseDragging();
    static Container* dragParentContainer = nullptr;
    static Element* draggedElement = nullptr;
    static int dragStartIndex = -1;

    if (alt) {
        for (auto& [name, component] : muloComponents) {
            if (component->getLayout() && component->isVisible()) {
                if (component->getLayout()->m_bounds.getGlobalBounds().contains(ui->getMousePosition())) {

                    if (dragParentContainer && component->getParentContainer() != dragParentContainer) {
                        dragOverlay.setSize({0.f, 0.f}); // Hide overlay if not in same container
                        continue;
                    }
                    else {
                        dragOverlay.setSize(component->getLayout()->m_bounds.getSize());
                        dragOverlay.setPosition(component->getLayout()->m_bounds.getPosition());
                        dragOverlay.setFillColor(sf::Color(255, 255, 255, 20));
                    }
                }
            }
        }
    } else {
        dragOverlay.setSize({0.f, 0.f}); // Hide overlay when not dragging
    }

    // On drag start: record dragged element and its parent container/index
    if (dragging && !prevDragging) {
        dragParentContainer = nullptr;
        draggedElement = nullptr;
        dragStartIndex = -1;
        for (auto& [name, component] : muloComponents) {
            if (component->getLayout() && component->isVisible()) {
                if (component->getLayout()->m_bounds.getGlobalBounds().contains(ui->getMousePosition())) {
                    Container* parent = component->getParentContainer();
                    Element* elem = component->getLayout();
                    int idx = parent ? parent->getElementIndex(elem) : -1;
                    if (parent && elem && idx != -1) {
                        dragParentContainer = parent;
                        draggedElement = elem;
                        dragStartIndex = idx;
                        std::cout << "Dragging component: " << name << " at index: " << dragStartIndex << std::endl;
                    } else {
                        std::cout << "Dragging component: " << name << " at index: -1 (not found in parent)" << std::endl;
                    }
                    break;
                }
            }
        }
    }

    // On drag end: find drop target and swap/move if valid
    if (!dragging && prevDragging && dragParentContainer && draggedElement && dragStartIndex != -1) {
        // Find the drop target element under the mouse (any container)
        Element* dropTarget = nullptr;
        Container* dropParentContainer = nullptr;
        int dropIndex = -1;
        std::string draggedComponentName, dropTargetComponentName;
        for (auto& [name, component] : muloComponents) {
            if (component->getLayout() && component->isVisible()) {
                if (component->getLayout()->m_bounds.getGlobalBounds().contains(ui->getMousePosition())) {
                    dropTarget = component->getLayout();
                    dropParentContainer = component->getParentContainer();
                    dropIndex = dropParentContainer ? dropParentContainer->getElementIndex(dropTarget) : -1;
                    dropTargetComponentName = name;
                    break;
                }
            }
        }
        // Find the name of the dragged component
        for (auto& [name, component] : muloComponents) {
            if (component->getLayout() == draggedElement) {
                draggedComponentName = name;
                break;
            }
        }
        if (dropTarget && dropParentContainer && dropIndex != -1) {
            if (dropParentContainer == dragParentContainer && dropIndex != dragStartIndex) {
                // Only allow swap within the same container
                dragParentContainer->swapElements(dragStartIndex, dropIndex);
                Align alignA = draggedElement->m_modifier.getAlignment();
                Align alignB = dropTarget->m_modifier.getAlignment();
                draggedElement->m_modifier.align(alignB);
                dropTarget->m_modifier.align(alignA);
                std::cout << "Swapped elements at indices: " << dragStartIndex << " <-> " << dropIndex << ", and alignments." << std::endl;

                // Update componentLayouts for both components
                for (auto& [name, component] : muloComponents) {
                    componentLayouts[name] = { 
                        (component->getParentContainer()) ? component->getParentContainer() : nullptr, 
                        (component->getLayout()) ? component->getLayout()->m_modifier.getAlignment() : Align::NONE, 
                        component->getRelativeTo() 
                    };
                }
            }
        }
        // Reset drag state
        dragParentContainer = nullptr;
        draggedElement = nullptr;
        dragStartIndex = -1;
    }

    prevDragging = dragging;
}

std::string Application::getAlignmentString(Align alignment) const {
    switch (alignment) {
        case Align::LEFT: return "LEFT";
        case Align::RIGHT: return "RIGHT";
        case Align::TOP: return "TOP";
        case Align::BOTTOM: return "BOTTOM";
        case Align::NONE: return "NONE";
        default: return "UNKNOWN";
    }
}

void Application::setComponentParentContainer(const std::string& componentName, Container* parent) {
    if (muloComponents.find(componentName) != muloComponents.end()) {
        muloComponents[componentName]->setParentContainer(parent);
    }
}
