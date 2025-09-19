

#include "Application.hpp"
#include "../audio/MIDIClip.hpp"

#include <tinyfiledialogs/tinyfiledialogs.hpp>
#include <filesystem>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#ifdef _WIN32
#include <windows.h>

static POINT s_minWindowSize = {800, 600};

LRESULT CALLBACK MinSizeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_GETMINMAXINFO) {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = s_minWindowSize.x;
        mmi->ptMinTrackSize.y = s_minWindowSize.y;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void SetMinWindowSize(HWND hwnd, int minWidth, int minHeight)
{
    s_minWindowSize.x = minWidth;
    s_minWindowSize.y = minHeight;
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)MinSizeWndProc);
}
#endif

#ifdef __APPLE__
#include <objc/objc.h>
#include <objc/message.h>
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace fs = std::filesystem;

Application::Application() {}

void Application::initialise(const juce::String& commandLine) {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    exeDirectory = fs::path(path).parent_path().string();
#else
    exeDirectory = fs::canonical("/proc/self/exe").parent_path().string();
#endif
    loadConfig();
    if (!uiState.vstDirecory.empty()) {
        engine.setVSTDirectory(uiState.vstDirecory);
    }
    if (!uiState.saveDirectory.empty()) {
        engine.setSampleDirectory(uiState.saveDirectory);
    }
    
    createWindow();
    applyTheme(resources, uiState.selectedTheme);
    initUIResources();
    initUI();

    engine.newComposition("untitled");
    engine.addTrack("Master");

    running = ui->isRunning();

    loadComponents();
    loadLayoutConfig();

    ui->setScale(1.f);

    ui->forceUpdate();
}

Application::~Application() {}

void Application::shutdown() {
    unloadAllPlugins();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void Application::update() {
    using namespace sf::Keyboard;
    using namespace sf::Mouse;
    using mb = sf::Mouse::Button;
    using kb = sf::Keyboard::Key;
    
    running = ui->isRunning();
    if (!running) return;
    
    handleEvents();

    bool rClick = isButtonPressed(mb::Right);
    bool lClick = isButtonPressed(mb::Left);
    bool ctrlShftR = isKeyPressed(kb::LControl) && isKeyPressed(kb::LShift) && isKeyPressed(kb::R);
    
    if (lClick || rClick) shouldForceUpdate = true;
    if (ctrlShftR && !prevCtrlShftR) rebuildUI();

    ui->forceUpdate();

    for (const auto& [name, component] : muloComponents)
        if (component)
            component->update();

    if (forceUpdatePoll > 0) --forceUpdatePoll;
    
    freshRebuild = false;
    prevCtrlShftR = ctrlShftR;
}

void Application::render() {
    if (ui->windowShouldUpdate()) {
        window.clear(sf::Color::Black);
        ui->render();
        window.draw(dragOverlay);
        window.display();
    }
}

void Application::handleEvents() {
    for (auto& component : muloComponents)
        shouldForceUpdate |= component.second->handleEvents();

    if (pendingUIRebuild) {
        rebuildUI();
        pendingUIRebuild = false;
    }

    if (pendingFullscreenToggle) {
        toggleFullscreen();
        pendingFullscreenToggle = false;
    }

    if (hasPendingEffect) {
        if (Effect::isVSTSynthesizer(pendingEffectPath)) {
            DEBUG_PRINT("Detected synthesizer VST, creating MIDI track: " << pendingEffectPath);
            juce::File vstFile(pendingEffectPath);
            std::string synthName = vstFile.getFileNameWithoutExtension().toStdString();
            std::string trackName = synthName;
            
            std::string actualTrackName = engine.addMIDITrack(trackName);
            MIDITrack* midiTrack = dynamic_cast<MIDITrack*>(engine.getTrackByName(actualTrackName));
            if (midiTrack) {
                Effect* synthEffect = midiTrack->addEffect(pendingEffectPath);
                if (synthEffect) {
                    synthEffect->enable();
                    synthEffect->openWindow();
                    engine.setSelectedTrack(actualTrackName);
                    DEBUG_PRINT("Created MIDI track '" << actualTrackName << "' with synthesizer: " << synthName);
                } else {
                    DEBUG_PRINT("Failed to add synthesizer to MIDI track: " << pendingEffectPath);
                }
            } else {
                DEBUG_PRINT("Failed to create MIDI track for synthesizer");
            }
        } else {
            Track* selectedTrack = getSelectedTrackPtr();
            if (selectedTrack) {
                Effect* effect = selectedTrack->addEffect(pendingEffectPath);
                if (effect) {
                    effect->openWindow();
                    
                    if (effect->isSynthesizer()) {
                        engine.sendBpmToSynthesizers();
                        DEBUG_PRINT("Sent BPM to newly loaded synthesizer effect: " << effect->getName());
                    }
                } else {
                    DEBUG_PRINT("Failed to load effect: " << pendingEffectPath);
                }
            } else {
                DEBUG_PRINT("No selected track for effect loading");
            }
        }
        hasPendingEffect = false;
        pendingEffectPath.clear();
    }

    if (hasPendingSynth) {
        juce::File vstFile(pendingSynthPath);
        std::string synthName = vstFile.getFileNameWithoutExtension().toStdString();
        std::string trackName = synthName + " Synth";
        
        engine.addMIDITrack(trackName);
        MIDITrack* midiTrack = dynamic_cast<MIDITrack*>(engine.getTrackByName(trackName));
        if (midiTrack) {
            Effect* synthEffect = midiTrack->addEffect(pendingSynthPath);
            if (synthEffect) {
                synthEffect->enable();
                synthEffect->openWindow();
                engine.setSelectedTrack(trackName);
                
                engine.sendBpmToSynthesizers();
                DEBUG_PRINT("Sent BPM to newly loaded synthesizer: " << synthName);
                DEBUG_PRINT("Created MIDI track '" << trackName << "' with synthesizer: " << synthName);
            } else {
                DEBUG_PRINT("Failed to add synthesizer to MIDI track: " << pendingSynthPath);
            }
        } else {
            DEBUG_PRINT("Failed to create MIDI track for synthesizer");
        }
        hasPendingSynth = false;
        pendingSynthPath.clear();
    }

    if (hasPendingEffectWindow) {
        Track* selectedTrack = getSelectedTrackPtr();
        if (selectedTrack) {
            auto& effects = selectedTrack->getEffects();
            if (pendingEffectWindowIndex < effects.size()) {
                effects[pendingEffectWindowIndex]->openWindow();
            } else {
                DEBUG_PRINT("Invalid effect index: " << pendingEffectWindowIndex);
            }
        } else {
            DEBUG_PRINT("No selected track for effect window opening");
        }
        hasPendingEffectWindow = false;
        pendingEffectWindowIndex = SIZE_MAX;
    }

    // Process deferred effects from save file loading (one per frame to spread load)
    if (hasDeferredEffects && !deferredEffects.empty()) {
        auto deferredEffect = deferredEffects.front();
        deferredEffects.erase(deferredEffects.begin());
        
        Track* targetTrack = nullptr;
        if (deferredEffect.trackName == "Master") {
            targetTrack = getMasterTrack();
        } else {
            targetTrack = getTrack(deferredEffect.trackName);
        }
        
        if (targetTrack) {
            Effect* effect = targetTrack->addEffect(deferredEffect.vstPath);
            if (effect) {
                if (!deferredEffect.enabled) {
                    effect->disable();
                }
                if (deferredEffect.index >= 0) {
                    effect->setIndex(deferredEffect.index);
                }
                
                for (const auto& paramPair : deferredEffect.parameters) {
                    effect->setParameter(paramPair.first, paramPair.second);
                }
                
                if (effect->isSynthesizer()) {
                    engine.sendBpmToSynthesizers();
                    DEBUG_PRINT("Sent BPM to deferred synthesizer: " << effect->getName());
                }
                
                if (effect->hasEditor()) {
                    effect->openWindow();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    effect->closeWindow();
                }
                
                DEBUG_PRINT("Loaded deferred effect: " << effect->getName() << " for track: " << deferredEffect.trackName);
            }
        } else {
            DEBUG_PRINT("Track not found for deferred effect: " << deferredEffect.trackName);
        }
        
        if (deferredEffects.empty()) {
            hasDeferredEffects = false;
            DEBUG_PRINT("All deferred effects loaded");
        }
    }

    const auto& enginePendingEffects = engine.getPendingEffects();
    if (!enginePendingEffects.empty()) {
        for (const auto& pendingEffect : enginePendingEffects) {
            DeferredEffect def;
            def.trackName = pendingEffect.trackName;
            def.vstPath = pendingEffect.vstPath;
            def.shouldOpenWindow = false;
            def.enabled = pendingEffect.enabled;
            def.index = pendingEffect.index;
            def.parameters = pendingEffect.parameters;
            
            deferredEffects.push_back(def);
        }
        
        hasDeferredEffects = !deferredEffects.empty();
        engine.clearPendingEffects();
        
        if (hasDeferredEffects) {
            DEBUG_PRINT("Processing " << deferredEffects.size() << " pending effects from save file" << std::endl);
        }
    }

    if (pendingTrackRemoveName != "") {
        auto track = engine.getTrackByName(pendingTrackRemoveName);
        
        if (track)
            track->clearEffects();
            
        DEBUG_PRINT("Removing track: " << pendingTrackRemoveName);
        engine.removeTrackByName(pendingTrackRemoveName);
        pendingTrackRemoveName = "";
    }

    handleDragAndDrop();
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
                    // don't highlight components not in the same parent container
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

void Application::setComponentParentContainer(const std::string& componentName, Container* parent) {
    if (muloComponents.find(componentName) != muloComponents.end()) {
        muloComponents[componentName]->setParentContainer(parent);
    }
}

void Application::initUI() {
    baseContainer = column(Modifier(), contains{}, "base_container");
    mainContentRow = row(Modifier().setWidth(1.f).setHeight(1.f).align(Align::BOTTOM), contains{}, "main_content_row");
    baseContainer->addElement(mainContentRow);
    
    uiloPages["base"] = page({baseContainer});
    ui = std::make_unique<UILO>(window, windowView);
    ui->addPage(page({baseContainer}), "base");
}

void Application::initUIResources() {
    auto findFont = [&](const std::string& filename) -> std::string {
        fs::path cwdFont = fs::current_path() / "assets" / "fonts" / filename;
        if (fs::exists(cwdFont)) return cwdFont.string();

        fs::path exeFont = fs::path(exeDirectory) / "assets" / "fonts" / filename;
        if (fs::exists(exeFont)) return exeFont.string();

        return "";
    };

    auto findIcon = [&](const std::string& filename) -> std::string {
        fs::path cwdIcon = fs::current_path() / "assets" / "icons" / filename;
        if (fs::exists(cwdIcon)) return cwdIcon.string();

        fs::path exeIcon = fs::path(exeDirectory) / "assets" / "icons" / filename;
        if (fs::exists(exeIcon)) return exeIcon.string();

        return "";
    };

    resources.dejavuSansFont    = findFont("DejaVuSans.ttf");
    resources.spaceMonoFont     = findFont("SpaceMono-Regular.ttf");
    resources.ubuntuBoldFont    = findFont("ubuntu.bold.ttf");
    resources.ubuntuMonoFont    = findFont("ubuntu.mono.ttf");
    resources.ubuntuMonoBoldFont= findFont("ubuntu.mono-bold.ttf");

    resources.playIcon          = sf::Image(findIcon("play.png"));
    resources.pauseIcon         = sf::Image(findIcon("pause.png"));
    resources.settingsIcon      = sf::Image(findIcon("settings.png"));
    resources.pianoRollIcon     = sf::Image(findIcon("piano.png"));
    resources.playIcon          = sf::Image(findIcon("play.png"));
    resources.pauseIcon         = sf::Image(findIcon("pause.png"));
    resources.settingsIcon      = sf::Image(findIcon("settings.png"));
    resources.pianoRollIcon     = sf::Image(findIcon("piano.png"));
    resources.loadIcon          = sf::Image(findIcon("load.png"));
    resources.saveIcon          = sf::Image(findIcon("save.png"));
    resources.exportIcon        = sf::Image(findIcon("export.png"));
    resources.folderIcon        = sf::Image(findIcon("folder.png"));
    resources.openFolderIcon    = sf::Image(findIcon("openfolder.png"));
    resources.pluginFileIcon    = sf::Image(findIcon("pluginfile.png"));
    resources.audioFileIcon     = sf::Image(findIcon("audiofile.png"));
    resources.metronomeIcon     = sf::Image(findIcon("metronome.png"));
    resources.mixerIcon         = sf::Image(findIcon("mixer.png"));
    resources.storeIcon         = sf::Image(findIcon("store.png"));
    resources.fileIcon          = sf::Image(findIcon("file.png"));
}

std::string Application::selectDirectory() {
    const char* dir = tinyfd_selectFolderDialog("Select Directory", exeDirectory.c_str());
    return dir ? std::string(dir) : "";
}

std::string Application::selectFile(std::initializer_list<std::string> filters) {
    std::vector<const char*> patterns;
    for (const auto& f : filters) patterns.push_back(f.c_str());
    const char* file = tinyfd_openFileDialog(
        "Select File",
        exeDirectory.c_str(),
        patterns.empty() ? 0 : patterns.size(),
        patterns.empty() ? nullptr : patterns.data(),
        nullptr,
        0
    );
    return file ? std::string(file) : "";
}

void Application::createWindow() {
    screenResolution = sf::VideoMode::getDesktopMode();
    screenResolution.size.x /= 1.5f;
    screenResolution.size.y /= 1.5f;
    minWindowSize.x = 800;
    minWindowSize.y = 600;

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 0;
    settings.depthBits = 0;
    settings.stencilBits = 0;
    settings.majorVersion = 1;
    settings.minorVersion = 0;
    settings.attributeFlags = sf::ContextSettings::Default;

    windowView.setSize({ (float)screenResolution.size.x / 2, (float)screenResolution.size.y / 2 });
    windowView.setCenter({ (float)screenResolution.size.x / 2.f, (float)screenResolution.size.y / 2.f });
    window.create(
        screenResolution, 
        "MULO", 
        sf::Style::Default, 
        (fullscreen) ? sf::State::Fullscreen : sf::State::Windowed,
        settings
    );
    window.setVerticalSyncEnabled(true);

    // Set minimum window size
    #ifdef __linux__
    Display* display = XOpenDisplay(nullptr);
    if (display) {
        XSizeHints hints;
        hints.flags = PMinSize;
        hints.min_width = minWindowSize.x;
        hints.min_height = minWindowSize.y;
        Window win = static_cast<Window>(window.getNativeHandle());
        XSetWMNormalHints(display, win, &hints);
        XCloseDisplay(display);
    }
    #endif

    // #ifdef _WIN32
    // HWND hwnd = (HWND)window.getNativeHandle();
    // SetMinWindowSize(hwnd, minWindowSize.x, minWindowSize.y);
    // #endif

    #ifdef __APPLE__
    void* nsWindow = window.getNativeHandle();
    if (nsWindow) {
        typedef void (*SetMinSizeFunc)(void*, SEL, CGSize);
        SetMinSizeFunc setMinSize = (SetMinSizeFunc)objc_msgSend;
        SEL sel = sel_registerName("setContentMinSize:");
        CGSize size = CGSizeMake(minWindowSize.x, minWindowSize.y);
        setMinSize(nsWindow, sel, size);
    }
    #endif
}

void Application::loadComponents() {
    // Scan and load plugin components
    scanAndLoadPlugins();

    // Initialize all components (built-in + plugins)
    for (auto& [name, component] : muloComponents) {
        if (!component) {
             std::cerr << "Error: Null component created for key: " << name << std::endl;
             continue;
        }
        component->setAppRef(this);
    }

    // Init MULO Components: loop until all are initialized or hit 15 attempts
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

void Application::rebuildUI() {
    unloadAllPlugins();
    muloComponents.clear();

    applyTheme(resources, uiState.selectedTheme);

    cleanup();
    initUI();
    loadComponents();

    freshRebuild = true; // Notify components that a rebuild happened
    forceUpdatePoll = 5;
}

void Application::toggleFullscreen() {
    fullscreen = !fullscreen;
    createWindow();
}

void Application::cleanup() {
    if (ui)
        ui->setFullClean(true);
    ui.reset();
    
    auto componentsToDestroy = std::move(muloComponents);
    muloComponents.clear();
    componentsToDestroy.clear();

    uiloPages.clear();
}

// Plugin System Implementation
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
            // if (pluginPath.find("Timeline") != std::string::npos) continue;
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
#ifdef _WIN32
        HMODULE handle = LoadLibraryA(pluginPath.c_str());
        if (!handle) {
            std::cerr << "Failed to load library: " << pluginPath << " (Error: " << GetLastError() << ")" << std::endl;
            return false;
        }

        // Get the plugin interface function
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

        // Clear any existing error
        dlerror();

        // Get the plugin interface function
        typedef PluginVTable* (*GetPluginInterfaceFunc)();
        GetPluginInterfaceFunc getPluginInterface = (GetPluginInterfaceFunc)dlsym(handle, "getPluginInterface");
        
        const char* dlsym_error = dlerror();
        if (dlsym_error) {
            std::cerr << "Plugin missing getPluginInterface function: " << pluginPath << " (" << dlsym_error << ")" << std::endl;
            dlclose(handle);
            return false;
        }
#endif

        // Get the plugin interface
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

        // Get plugin name (getName needs instance parameter)
        const char* pluginName = vtable->getName(vtable->instance);
        if (!pluginName) {
            std::cerr << "Plugin name is null: " << pluginPath << std::endl;
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return false;
        }

        std::string name(pluginName);
        
        // Check if plugin with this name is already loaded
        if (muloComponents.find(name) != muloComponents.end()) {
            DEBUG_PRINT("Plugin with name '" << name << "' already loaded, skipping");
#ifdef _WIN32
            FreeLibrary(handle);
#else
            dlclose(handle);
#endif
            return false;
        }

        // Create wrapper component
        auto wrapper = std::make_unique<PluginComponentWrapper>(vtable);
        
        // Store the loaded plugin info
        LoadedPlugin loadedPlugin;
        loadedPlugin.path = pluginPath;
        loadedPlugin.handle = handle;
        loadedPlugin.plugin = vtable;
        loadedPlugin.name = name;
        loadedPlugins[name] = std::move(loadedPlugin);

        // Add to components map
        muloComponents[name] = std::move(wrapper);
        
        std::cout << "Plugin '" << name << "' loaded successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception loading plugin " << pluginPath << ": " << e.what() << std::endl;
        return false;
    }
}

void Application::unloadPlugin(const std::string& pluginName) {
    // Remove from components map first - this destroys the wrapper and calls plugin cleanup
    auto componentIt = muloComponents.find(pluginName);
    if (componentIt != muloComponents.end()) {
        // Defensive: set plugin pointer to nullptr before erasing
        if (auto* wrapper = dynamic_cast<PluginComponentWrapper*>(componentIt->second.get())) {
            wrapper->plugin = nullptr;
        }
        componentIt->second.reset();
        muloComponents.erase(componentIt);
    }

    // Give a moment for any async cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Find and unload the plugin
    auto it = loadedPlugins.find(pluginName);
    if (it != loadedPlugins.end()) {
        if (it->second.plugin && it->second.plugin->destroy) {
            it->second.plugin->destroy(it->second.plugin->instance);
        }

        // Give time for destructor cleanup
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Unload the library
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
    // Create a copy of plugin names to avoid iterator invalidation
    std::vector<std::string> pluginNames;
    for (const auto& plugin : loadedPlugins) {
        pluginNames.push_back(plugin.first);  // first is the key (name)
    }

    // Unload each plugin
    for (const auto& name : pluginNames) {
        unloadPlugin(name);
    }

    sliders.clear();
    containers.clear();
    texts.clear();
    spacers.clear();
    buttons.clear();
    dropdowns.clear();
    uilo_owned_elements.clear();
    high_priority_elements.clear();
}

void Application::saveLayoutConfig() {
    // Save componentLayouts to layout.json in exeDirectory using nlohmann_json
    nlohmann::json j;
    for (const auto& [name, layout] : componentLayouts) {
        std::string parentName = layout.parent ? layout.parent->m_name : "";
        j[name] = {
            {"parent", parentName},
            {"alignment", static_cast<int>(layout.alignment)},
            {"relativeTo", layout.relativeTo}
        };
    }
    std::string path = exeDirectory + "/layout.json";
    std::ofstream ofs(path);
    if (ofs.is_open()) {
        ofs << j.dump(4);
        ofs.close();
        std::cout << "Layout saved to: " << path << std::endl;
    } else {
        std::cerr << "Failed to open layout.json for writing: " << path << std::endl;
    }
}

void Application::loadLayoutConfig() {
    std::string path = exeDirectory + "/layout.json";
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open layout.json for reading: " << path << std::endl;
        return;
    }
    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing layout.json: " << e.what() << std::endl;
        return;
    }

    std::unordered_map<std::string, Container*> containerMap;
    for (auto& [name, component] : muloComponents) {
        if (auto* c = dynamic_cast<Container*>(component.get())) {
            containerMap[name] = c;
        }
    }

    for (auto& [name, layoutData] : j.items()) {
        if (muloComponents.find(name) == muloComponents.end()) continue;
        auto& component = muloComponents[name];
        auto& layout = componentLayouts[name];

        // Parent
        std::string parentName = layoutData.value("parent", "");
        Container* parent = nullptr;
        if (!parentName.empty() && containerMap.count(parentName)) {
            parent = containerMap[parentName];
            component->setParentContainer(parent);
            layout.parent = parent;
        }

        // Alignment
        int alignInt = layoutData.value("alignment", static_cast<int>(Align::NONE));
        Align align = static_cast<Align>(alignInt);
        if (component->getLayout()) {
            component->getLayout()->m_modifier.align(align);
        }
        layout.alignment = align;

        // RelativeTo
        std::string relTo = layoutData.value("relativeTo", "");
        component->setRelativeTo(relTo);
        layout.relativeTo = relTo;
    }
    std::cout << "Layout loaded from: " << path << std::endl;
}

void Application::saveConfig() {
    try {
        std::string configPath = exeDirectory + "/config.json";
        std::ofstream file(configPath);
        if (!file.is_open()) {
            DEBUG_PRINT("Failed to open config file for writing: " << configPath);
            return;
        }
        
        file << config.dump(2); // Pretty print with 2-space indentation
        file.close();
        
        DEBUG_PRINT("Configuration saved to: " << configPath);
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error saving config: " << e.what());
    }
}

void Application::loadConfig() {
    try {
        std::string configPath = exeDirectory + "/config.json";
        std::ifstream file(configPath);
        
        if (!file.is_open()) {
            DEBUG_PRINT("Config file not found, using defaults: " << configPath);
            return;
        }

        file >> config;
        file.close();
        
        DEBUG_PRINT("Configuration loaded from: " << configPath);
    } catch (const nlohmann::json::parse_error& e) {
        DEBUG_PRINT("JSON parse error loading config: " << e.what());
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error loading config: " << e.what());
    }
}

MIDIClip* Application::getSelectedMIDIClip() const {
    std::string selectedTrackName = getSelectedTrack();
    if (selectedTrackName.empty()) return nullptr;
    
    Track* selectedTrack = const_cast<Application*>(this)->getTrack(selectedTrackName);
    if (selectedTrack && selectedTrack->getType() == Track::TrackType::MIDI) {
        MIDITrack* midiTrack = static_cast<MIDITrack*>(selectedTrack);
        const auto& midiClips = midiTrack->getMIDIClips();
        
        if (!midiClips.empty()) {
            return const_cast<MIDIClip*>(&midiClips[0]);
        }
    }
    
    return nullptr;
}

MIDIClip* Application::getTimelineSelectedMIDIClip() const {
    if (auto* timelineComponent = const_cast<Application*>(this)->getComponent("timeline")) {
        MIDIClip* result = timelineComponent->getSelectedMIDIClip();
        DEBUG_PRINT("[APP] getTimelineSelectedMIDIClip() returning: " << (result ? 
            ("startTime=" + std::to_string(result->startTime) + ", duration=" + std::to_string(result->duration)) : 
            "nullptr"));
        return result;
    }
    
    DEBUG_PRINT("[APP] getTimelineSelectedMIDIClip() - timeline component not found");
    return nullptr;
}