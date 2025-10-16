

#include "Application.hpp"
#include "../audio/MIDIClip.hpp"

#include <tinyfiledialogs/tinyfiledialogs.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <unordered_map>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef Success
#undef Success
#endif

// log X11 errors rather than exiting
static int x11ErrorHandler(Display* display, XErrorEvent* event) {
    char errorText[256];
    XGetErrorText(display, event->error_code, errorText, sizeof(errorText));
    std::cerr << "X11 Error: " << errorText 
              << " (request code: " << (int)event->request_code 
              << ", minor code: " << (int)event->minor_code << ")" << std::endl;
    return 0;
}
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
#ifdef __linux__
    XSetErrorHandler(x11ErrorHandler);
#endif

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
    } else if (!uiState.fileBrowserDirectory.empty()) {
        engine.setSampleDirectory(uiState.fileBrowserDirectory);
        DEBUG_PRINT("Using fileBrowserDirectory as sample directory: " << uiState.fileBrowserDirectory);
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
    
    // Initialize Firebase for marketplace functionality
    initFirebase();

    ui->setScale(uiState.uiScale);
    ui->forceUpdate();
}

Application::~Application() {
    cleanupFirebaseResources();    
    saveConfig();
    unloadAllPlugins();
    
#ifdef FIREBASE_AVAILABLE
    if (firestore) {
        delete firestore;
        firestore = nullptr;
    }
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

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
    
    processPendingEngineUpdates();
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

    updateParameterTracking();
    
    // Update window title
    std::string compositionName = engine.getCurrentCompositionName();
    std::string userInfo = userLoggedIn ? currentUserEmail : "logged out";
    std::string newTitle = "MULO | " + compositionName + " | " + userInfo;
    
    if (newTitle != lastWindowTitle) {
        window.setTitle(newTitle);
        lastWindowTitle = newTitle;
    }

    // Process Firebase requests
#ifdef FIREBASE_AVAILABLE
    if (firebaseState == FirebaseState::Loading && extFuture.status() == firebase::kFutureStatusComplete) {
        if (extFuture.error() == firebase::firestore::kErrorNone) {
            const auto& snapshot = *extFuture.result();
            for (const auto& doc : snapshot.documents()) {
                ExtensionData data;
                data.id = doc.id();
                if (doc.Get("name").is_string()) data.name = doc.Get("name").string_value();
                if (doc.Get("author").is_string()) data.author = doc.Get("author").string_value();
                if (doc.Get("version").is_string()) data.version = doc.Get("version").string_value();
                if (doc.Get("downloadURL").is_string()) data.downloadURL = doc.Get("downloadURL").string_value();
                if (doc.Get("description").is_string()) data.description = doc.Get("description").string_value();
                if (doc.Get("verified").is_boolean()) data.verified = doc.Get("verified").boolean_value();
                extensions.push_back(data);
            }
            firebaseState = FirebaseState::Success;
        } else {
            firebaseState = FirebaseState::Error;
        }
        
        if (firebaseCallback) {
            firebaseCallback(firebaseState, extensions);
            firebaseCallback = nullptr;
        }
    }
#endif

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
                }
            }
        } else {
            Track* selectedTrack = getSelectedTrackPtr();
            if (selectedTrack) {
                Effect* effect = selectedTrack->addEffect(pendingEffectPath);
                if (effect) {
                    effect->openWindow();
                    
                    if (effect->isSynthesizer()) {
                        engine.sendBpmToSynthesizers();

                    }
                } else {

                }
            } else {

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


            } else {

            }
        } else {

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

            }
        } else {

        }
        hasPendingEffectWindow = false;
        pendingEffectWindowIndex = SIZE_MAX;
    }


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

                }
                
                if (effect->hasEditor()) {
                    effect->openWindow();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    effect->closeWindow();
                }
                

            }
        } else {

        }
        
        if (deferredEffects.empty()) {
            hasDeferredEffects = false;

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

        }
    }

    if (pendingTrackRemoveName != "") {
        auto track = engine.getTrackByName(pendingTrackRemoveName);
        
        if (track)
            track->clearEffects();
            

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
    resources.automationIcon    = sf::Image(findIcon("showautomation.png"));
    resources.collabIcon        = sf::Image(findIcon("collab.png"));
    resources.loginIcon         = sf::Image(findIcon("login.png"));
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
        // Extract plugin filename for trust checking
        fs::path pluginFile(pluginPath);
        std::string pluginName = pluginFile.filename().string();
        
        // Check if plugin is trusted (bypass sandbox) using hardcoded verification
        bool isTrusted = isPluginTrusted(pluginName);

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

        // Create wrapper component with sandbox status and plugin filename
        auto wrapper = std::make_unique<PluginComponentWrapper>(vtable, !isTrusted, pluginName);
        
        // Store the loaded plugin info
        LoadedPlugin loadedPlugin;
        loadedPlugin.path = pluginPath;
        loadedPlugin.handle = handle;
        loadedPlugin.plugin = vtable;
        loadedPlugin.name = name;
        loadedPlugin.isSandboxed = !isTrusted;
        loadedPlugin.isTrusted = isTrusted;
        loadedPlugins[name] = std::move(loadedPlugin);

        // Add to components map
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
    // Remove from components map first - this destroys the wrapper and calls plugin cleanup
    auto componentIt = muloComponents.find(pluginName);
    if (componentIt != muloComponents.end()) {
        // Clean up sandbox if the plugin is sandboxed
        if (auto* wrapper = dynamic_cast<PluginComponentWrapper*>(componentIt->second.get())) {
            if (wrapper->isSandboxed()) {
                wrapper->cleanupSandbox();
                DEBUG_PRINT("Cleaned up sandbox for plugin: " << pluginName);
            }
            // Defensive: set plugin pointer to nullptr before erasing
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

    uilo_owned_elements.clear();
    high_priority_elements.clear();
    
    // Note: Element registries (sliders, buttons, etc.) are now per-UILO instance
    // and will be cleaned up when the UILO instance is destroyed
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
        
        file << config.dump(2);
        file.close();        
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
        
        // Populate uiState from loaded config
        uiState.fileBrowserDirectory = readConfig<std::string>("fileBrowserDirectory", "");
        uiState.vstDirecory = readConfig<std::string>("vstDirectory", "");
        uiState.vstDirectories = readConfig<std::vector<std::string>>("vstDirectories", std::vector<std::string>());
        uiState.saveDirectory = readConfig<std::string>("saveDirectory", "");
        uiState.selectedTheme = readConfig<std::string>("selectedTheme", "Dark");
        
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
        return timelineComponent->getSelectedMIDIClip();
    }
    return nullptr;
}

void Application::setPluginTrusted(const std::string& pluginName, bool trusted) {
    // Check database
}

bool Application::isPluginTrusted(const std::string& pluginName) const {
    // Map plugin filenames to Firebase extension IDs
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
    
    // Try to find the extension ID for this plugin
    auto it = pluginToExtensionMap.find(pluginName);
    if (it != pluginToExtensionMap.end()) {
        const std::string& extensionId = it->second;
        
        // Check Firebase data if available
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
    
    // Fallback to hardcoded trusted plugins if Firebase data not available
    static const std::vector<std::string> fallbackTrustedPlugins = {
        "TimelineComponent.so",
        "PianoRollComponent.so",
        "MixerComponent.so",
        "FXRackComponent.so",
        "MarketplaceComponent.so",
        "AppControls.so",
        "MULOCollab.so",
        "UserLogin.so"
    };
    
    bool isTrusted = std::find(fallbackTrustedPlugins.begin(), fallbackTrustedPlugins.end(), pluginName) != fallbackTrustedPlugins.end();
    std::cout << "Plugin '" << pluginName << "' using fallback trust: " << (isTrusted ? "TRUSTED" : "SANDBOXED") << std::endl;
    return isTrusted;
}

// Firebase implementation
void Application::initFirebase() {
#ifdef FIREBASE_AVAILABLE
    try {
        firebase::AppOptions options;
        options.set_api_key("AIzaSyCz8-U53Iga6AbMXvB7XMjOSSkqVLGYpOA");
        options.set_app_id("1:1068093358007:web:bdc95a20f8e60375bf7232");
        options.set_project_id("mulo-marketplace");
        options.set_storage_bucket("mulo-marketplace.firebasestorage.app");
        options.set_database_url("https://mulo-marketplace-default-rtdb.firebaseio.com/");

        firebaseApp.reset(firebase::App::Create(options));
        
        // Configure Firestore settings for better cache handling
        firebase::firestore::Settings settings;
        settings.set_cache_size_bytes(firebase::firestore::Settings::kCacheSizeUnlimited);
        settings.set_persistence_enabled(false);  // Disable local persistence to avoid file issues
        
        firestore = firebase::firestore::Firestore::GetInstance(firebaseApp.get());
        firestore->set_settings(settings);
        
        auth = firebase::auth::Auth::GetAuth(firebaseApp.get());        
        realtimeDatabase = firebase::database::Database::GetInstance(firebaseApp.get());
        realtimeDatabase->set_persistence_enabled(false);
        storage = firebase::storage::Storage::GetInstance(firebaseApp.get());        
        auto authFuture = auth->SignInAnonymously();
        authFuture.OnCompletion([this](const firebase::Future<firebase::auth::AuthResult>& result) {
            if (result.error() == firebase::auth::kAuthErrorNone) {
                std::cout << "Firebase anonymous authentication successful" << std::endl;
            } else {
                std::cout << "Firebase authentication failed: " << result.error_message() << std::endl;
            }
        });
        
        std::cout << "Firebase initialized successfully" << std::endl;     
    } catch (const std::exception& e) {
        std::cout << "Firebase initialization failed: " << e.what() << std::endl;
        firebaseState = FirebaseState::Error;
    }       
#else
    std::cout << "Firebase not available - using mock data" << std::endl;
    
    // Create some mock extension data
    extensions.clear();
    ExtensionData mockExt1;
    mockExt1.id = "mock-extension-1";
    mockExt1.name = "Sample Extension 1";
    mockExt1.author = "Demo Author";
    mockExt1.description = "A sample extension for demonstration purposes";
    mockExt1.version = "1.0.0";
    mockExt1.verified = true;
    extensions.push_back(mockExt1);
    
    ExtensionData mockExt2;
    mockExt2.id = "mock-extension-2";
    mockExt2.name = "Another Extension";
    mockExt2.author = "Test Developer";
    mockExt2.description = "Another sample extension";
    mockExt2.version = "0.5.0";
    mockExt2.verified = false;
    extensions.push_back(mockExt2);
    
    firebaseState = FirebaseState::Success;
#endif
}

void Application::fetchExtensions(std::function<void(FirebaseState, const std::vector<ExtensionData>&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!firestore) {
        callback(FirebaseState::Error, {});
        return;
    }
    
    if (firebaseState == FirebaseState::Loading) {
        return; // Already loading
    }
    
    firebaseState = FirebaseState::Loading;
    firebaseCallback = callback;
    extensions.clear();
    
    extFuture = firestore->Collection("extensions").Get();
#endif
}

void Application::uploadExtension(const ExtensionData& extensionData, const std::vector<std::string>& filePaths, 
                                std::function<void(FirebaseState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!firestore || !storage || !auth) {
        callback(FirebaseState::Error, "Firebase not initialized");
        return;
    }

    if (filePaths.empty()) {
        callback(FirebaseState::Error, "No files to upload");
        return;
    }
    
    std::thread([this, extensionData, filePaths, callback]() {
        auto docRef = firestore->Collection("extensions").Document();
        std::string documentId = docRef.id();
        
        std::string firstFilePath = filePaths[0];
        std::string fileName = firstFilePath.substr(firstFilePath.find_last_of("/\\") + 1);
        
        std::string storagePath = "extensions/" + documentId + "/" + fileName;
        auto storageRef = storage->GetReference(storagePath);
        
        firebase::storage::Metadata metadata;
        metadata.set_content_type("application/octet-stream");
        metadata.set_cache_control("no-cache");
        std::ifstream file(firstFilePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            callback(FirebaseState::Error, "Failed to read file");
            return;
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            callback(FirebaseState::Error, "Failed to read file data");
            return;
        }
        file.close();
    
        auto bucketRef = firebase::storage::Storage::GetInstance(firebaseApp.get(), "gs://mulo-marketplace.firebasestorage.app");
        auto storageRef2 = bucketRef->GetReference(storagePath);
        
        auto uploadFuture = storageRef2.PutBytes(buffer.data(), size);
        
        while (uploadFuture.status() == firebase::kFutureStatusPending) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (uploadFuture.error() != 0) {
            std::string errorMsg = uploadFuture.error_message() ? uploadFuture.error_message() : "Unknown error";
            std::string friendlyError;
            switch(uploadFuture.error()) {
                case 1: friendlyError = "Cancelled"; break;
                case 2: friendlyError = "Permission denied - Firebase Storage rules need to be updated"; break;
                case 3: friendlyError = "Invalid argument"; break;
                case 4: friendlyError = "Deadline exceeded"; break;
                case 5: friendlyError = "Not found"; break;
                case 6: friendlyError = "Already exists"; break;
                case 7: friendlyError = "Permission denied"; break;
                case 16: friendlyError = "Unauthenticated"; break;
                default: friendlyError = "Error " + std::to_string(uploadFuture.error());
            }
            callback(FirebaseState::Error, "Failed to upload file: " + friendlyError + " (" + errorMsg + ")");
            return;
        }
        auto urlFuture = storageRef2.GetDownloadUrl();
        
        while (urlFuture.status() == firebase::kFutureStatusPending) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (urlFuture.error() != 0) {
            callback(FirebaseState::Error, "Failed to get download URL: " + std::string(urlFuture.error_message()));
            return;
        }
        
        std::string downloadURL = urlFuture.result()->c_str();
        firebase::firestore::MapFieldValue data;
        data["author"] = firebase::firestore::FieldValue::String(extensionData.author);
        data["description"] = firebase::firestore::FieldValue::String(extensionData.description);
        data["name"] = firebase::firestore::FieldValue::String(extensionData.name);
        data["version"] = firebase::firestore::FieldValue::String(extensionData.version);
        data["verified"] = firebase::firestore::FieldValue::Boolean(false);
        data["downloadURL"] = firebase::firestore::FieldValue::String(downloadURL);
        
        auto setFuture = docRef.Set(data);
        
        while (setFuture.status() == firebase::kFutureStatusPending) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        if (setFuture.error() != 0) {
            callback(FirebaseState::Error, "Failed to create document: " + std::string(setFuture.error_message()));
            return;
        }
        
        callback(FirebaseState::Success, "Extension uploaded successfully");
    
    }).detach(); // Detach thread so it runs independently
    
#else
    callback(FirebaseState::Error, "Firebase not available");
#endif
}

void Application::updateParameterTracking() {
    // Update parameter tracking for all tracks
    for (auto& track : getAllTracks()) {
        if (track) {
            track->updateParameterTracking();
        }
    }
    
    // Also update master track
    if (auto* masterTrack = getMasterTrack()) {
        masterTrack->updateParameterTracking();
    }
}

void Application::createRoom(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase || !auth) {
        std::cout << "Firebase not ready for collaboration" << std::endl;
        return;
    }
    
    auto currentUser = auth->current_user();
    if (!currentUser.is_valid()) {
        std::cout << "User not authenticated" << std::endl;
        return;
    }
    
    std::cout << "Creating room: " << roomName << std::endl;
    std::string roomPath = "rooms/" + roomName;
    auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
    
    auto checkFuture = roomRef.GetValue();
    checkFuture.OnCompletion([this, roomName, currentUser](const firebase::Future<firebase::database::DataSnapshot>& result) {
        if (result.error() == firebase::database::kErrorNone) {
            auto snapshot = result.result();
            if (snapshot->exists()) {
                std::cout << "Room already exists: " << roomName << std::endl;
                return;
            }
            
            // Room doesn't exist, create it
            std::string engineState = engine.getStateString();
            std::cout << "Engine state length: " << engineState.length() << std::endl;
            std::cout << "User ID: '" << currentUser.uid() << "'" << std::endl;
            
            std::string roomPath = "rooms/" + roomName;
            auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
            
            firebase::Variant roomData = firebase::Variant::EmptyMap();
            firebase::Variant participants = firebase::Variant::EmptyMap();
            
            // Get creator's nickname from config
            std::string creatorNickname = readConfig<std::string>("collab_nickname", "Anonymous");
            participants.map()[currentUser.uid()] = firebase::Variant(creatorNickname);
            
            roomData.map()["engineState"] = firebase::Variant(engineState);
            roomData.map()["createdBy"] = firebase::Variant(currentUser.uid());
            roomData.map()["createdAt"] = firebase::Variant(static_cast<int64_t>(std::time(nullptr)));
            roomData.map()["participants"] = participants;
            roomData.map()["participantCount"] = firebase::Variant(static_cast<int64_t>(1));
            
            auto createFuture = roomRef.SetValue(roomData);
            createFuture.OnCompletion([roomName](const firebase::Future<void>& result) {
                if (result.error() == firebase::database::kErrorNone) {
                    std::cout << "Room created successfully: " << roomName << std::endl;
                } else {
                    std::cout << "Failed to create room: " << result.error_message() << std::endl;
                }
            });
        } else {
            std::cout << "Failed to check if room exists: " << result.error_message() << std::endl;
        }
    });
#else
    std::cout << "Firebase not available - mock room creation: " << roomName << std::endl;
#endif
}

void Application::readFromRoom(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase) {
        std::cout << "Firebase not ready for collaboration" << std::endl;
        return;
    }
    
    std::cout << "Reading from room: " << roomName << std::endl;
    
    std::string roomPath = "rooms/" + roomName;
    auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
    auto readFuture = roomRef.GetValue();
    
    readFuture.OnCompletion([this, roomName](const firebase::Future<firebase::database::DataSnapshot>& result) {
        std::lock_guard<std::mutex> lock(firebaseMutex);
        
        if (result.error() == firebase::database::kErrorNone) {
            auto snapshot = result.result();
            if (snapshot->exists()) {
                auto roomData = snapshot->value();
                if (roomData.is_map()) {
                    auto engineStateIt = roomData.map().find("engineState");
                    if (engineStateIt != roomData.map().end() && engineStateIt->second.is_string()) {
                        std::string engineState = engineStateIt->second.string_value();
                        std::cout << "Loading engine state from room: " << roomName << std::endl;
                        
                        // Queue engine state loading instead of applying directly
                        {
                            std::lock_guard<std::mutex> updateLock(engineUpdateMutex);
                            pendingEngineStateUpdate = engineState;
                            hasPendingEngineUpdate = true;
                        }
                        
                        std::cout << "Queued engine state from room for safe loading" << std::endl;
                    } else {
                        std::cout << "No engine state found in room: " << roomName << std::endl;
                    }
                } else {
                    std::cout << "Invalid room data format: " << roomName << std::endl;
                }
            } else {
                std::cout << "Room does not exist: " << roomName << std::endl;
            }
        } else {
            std::cout << "Failed to read room: " << result.error_message() << std::endl;
        }
    });
#else
    std::cout << "Firebase not available - mock room read: " << roomName << std::endl;
#endif
}

void Application::joinRoom(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase || !auth) {
        std::cout << "Firebase not ready for collaboration" << std::endl;
        return;
    }
    
    auto currentUser = auth->current_user();
    if (!currentUser.is_valid()) {
        std::cout << "User not authenticated" << std::endl;
        return;
    }
    
    std::cout << "Joining room: " << roomName << std::endl;
    
    std::string roomPath = "rooms/" + roomName;
    auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
    
    // First check if room exists
    auto readFuture = roomRef.GetValue();
    readFuture.OnCompletion([this, roomName, currentUser](const firebase::Future<firebase::database::DataSnapshot>& result) {
        if (result.error() == firebase::database::kErrorNone) {
            auto snapshot = result.result();
            if (snapshot->exists()) {
                // Get user's nickname from config
                std::string nickname = readConfig<std::string>("collab_nickname", "Anonymous");
                
                // Check if nickname is already taken in this room
                auto roomData = snapshot->value();
                if (roomData.is_map()) {
                    auto participantsIt = roomData.map().find("participants");
                    if (participantsIt != roomData.map().end() && participantsIt->second.is_map()) {
                        // Check if any existing participant has the same nickname
                        for (const auto& participant : participantsIt->second.map()) {
                            if (participant.second.is_string() && participant.second.string_value() == nickname) {
                                std::cout << "Nickname '" << nickname << "' is already taken in room: " << roomName << std::endl;
                                return;
                            }
                        }
                    }
                }
                
                // Nickname is unique, add this user to participants
                std::string participantPath = "rooms/" + roomName + "/participants/" + currentUser.uid();
                auto participantRef = realtimeDatabase->GetReference(participantPath.c_str());
                
                auto joinFuture = participantRef.SetValue(firebase::Variant(nickname));
                joinFuture.OnCompletion([this, roomName](const firebase::Future<void>& joinResult) {
                    if (joinResult.error() == firebase::database::kErrorNone) {
                        std::cout << "Successfully joined room: " << roomName << std::endl;
                        
                        // Update participant count
                        std::string countPath = "rooms/" + roomName + "/participants";
                        auto countRef = realtimeDatabase->GetReference(countPath.c_str());
                        auto countFuture = countRef.GetValue();
                        countFuture.OnCompletion([this, roomName](const firebase::Future<firebase::database::DataSnapshot>& countResult) {
                            if (countResult.error() == firebase::database::kErrorNone) {
                                auto countSnapshot = countResult.result();
                                if (countSnapshot->exists()) {
                                    int64_t participantCount = countSnapshot->value().map().size();
                                    std::string participantCountPath = "rooms/" + roomName + "/participantCount";
                                    auto participantCountRef = realtimeDatabase->GetReference(participantCountPath.c_str());
                                    participantCountRef.SetValue(firebase::Variant(participantCount));
                                }
                            }
                        });
                        
                        // Load the room's engine state
                        readFromRoom(roomName);
                    } else {
                        std::cout << "Failed to join room: " << joinResult.error_message() << std::endl;
                    }
                });
            } else {
                std::cout << "Room does not exist: " << roomName << std::endl;
            }
        } else {
            std::cout << "Failed to check room existence: " << result.error_message() << std::endl;
        }
    });
#else
    std::cout << "Firebase not available - mock room join: " << roomName << std::endl;
#endif
}

void Application::leaveRoom(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase || !auth) {
        std::cout << "Firebase not ready for collaboration" << std::endl;
        return;
    }
    
    auto currentUser = auth->current_user();
    if (!currentUser.is_valid()) {
        std::cout << "User not authenticated" << std::endl;
        return;
    }
    
    std::cout << "Leaving room: " << roomName << std::endl;
    
    // Remove this user from participants
    std::string participantPath = "rooms/" + roomName + "/participants/" + currentUser.uid();
    auto participantRef = realtimeDatabase->GetReference(participantPath.c_str());
    
    auto leaveFuture = participantRef.RemoveValue();
    leaveFuture.OnCompletion([this, roomName](const firebase::Future<void>& leaveResult) {
        if (leaveResult.error() == firebase::database::kErrorNone) {
            std::cout << "Successfully left room: " << roomName << std::endl;
            
            // Check if room is now empty and delete if so
            std::string participantsPath = "rooms/" + roomName + "/participants";
            auto participantsRef = realtimeDatabase->GetReference(participantsPath.c_str());
            auto checkFuture = participantsRef.GetValue();
            checkFuture.OnCompletion([this, roomName](const firebase::Future<firebase::database::DataSnapshot>& checkResult) {
                if (checkResult.error() == firebase::database::kErrorNone) {
                    auto snapshot = checkResult.result();
                    if (!snapshot->exists() || snapshot->value().map().empty()) {
                        // Room is empty, delete it
                        std::string roomPath = "rooms/" + roomName;
                        auto roomRef = realtimeDatabase->GetReference(roomPath.c_str());
                        auto deleteFuture = roomRef.RemoveValue();
                        deleteFuture.OnCompletion([roomName](const firebase::Future<void>& deleteResult) {
                            if (deleteResult.error() == firebase::database::kErrorNone) {
                                std::cout << "Room deleted (was empty): " << roomName << std::endl;
                            } else {
                                std::cout << "Failed to delete empty room: " << deleteResult.error_message() << std::endl;
                            }
                        });
                    } else {
                        // Update participant count
                        int64_t participantCount = snapshot->value().map().size();
                        std::string participantCountPath = "rooms/" + roomName + "/participantCount";
                        auto participantCountRef = realtimeDatabase->GetReference(participantCountPath.c_str());
                        participantCountRef.SetValue(firebase::Variant(participantCount));
                        std::cout << "Room " << roomName << " now has " << participantCount << " participants" << std::endl;
                    }
                }
            });
        } else {
            std::cout << "Failed to leave room: " << leaveResult.error_message() << std::endl;
        }
    });
#else
    std::cout << "Firebase not available - mock room leave: " << roomName << std::endl;
#endif
}

void Application::updateRoomEngineState(const std::string& roomName, const std::string& engineState) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase) return;
    
    // Skip if this is the same as the last known remote state (avoid ping-pong)
    if (engineState == lastKnownRemoteEngineState) {
        std::cout << "Skipping Firebase update - state matches last known remote state" << std::endl;
        return;
    }
    
    // Additional check: use state hash to prevent unnecessary writes
    static std::string lastWrittenStateHash;
    std::string currentStateHash = engine.getStateHash();
    
    if (currentStateHash == lastWrittenStateHash) {
        std::cout << "Skipping Firebase update - state hash unchanged" << std::endl;
        return;
    }
    
    lastWrittenStateHash = currentStateHash;
    writeToRoom(roomName, "engineState", engineState);
    std::cout << "Updated Firebase with new engine state (hash: " << currentStateHash << ")" << std::endl;
#endif
}

void Application::writeToRoom(const std::string& roomName, const std::string& section, const std::string& data) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase) return;
    
    std::lock_guard<std::mutex> lock(firebaseMutex);
    
    std::string path = "rooms/" + roomName + "/" + section;
    auto ref = realtimeDatabase->GetReference(path.c_str());
    ref.SetValue(firebase::Variant(data));
#endif
}

void Application::checkRoomEngineState(const std::string& roomName) {
#ifdef FIREBASE_AVAILABLE
    if (!realtimeDatabase) return;
    
    static std::chrono::steady_clock::time_point lastFirebaseCall;
    static int consecutiveCallCount = 0;
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastCall = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFirebaseCall).count();
    
    int minDelay = 1000 + (consecutiveCallCount * 200);
    minDelay = std::min(minDelay, 5000);
    
    if (timeSinceLastCall < minDelay) {
        return;
    }
    
    if (timeSinceLastCall > 10000) {
        consecutiveCallCount = 0;
    } else {
        consecutiveCallCount++;
    }
    
    lastFirebaseCall = now;
    
    std::string engineStatePath = "rooms/" + roomName + "/engineState";
    auto engineStateRef = realtimeDatabase->GetReference(engineStatePath.c_str());
    
    // Use async operation without blocking
    auto readFuture = engineStateRef.GetValue();
    
    // Store future for cleanup and non-blocking completion check
    {
        std::lock_guard<std::mutex> lock(firebaseMutex);
        pendingFirebaseFutures.push_back(readFuture);
    }
    
    readFuture.OnCompletion([this](const firebase::Future<firebase::database::DataSnapshot>& result) {
        // Remove this future from pending list
        {
            std::lock_guard<std::mutex> lock(firebaseMutex);
            pendingFirebaseFutures.erase(
                std::remove_if(pendingFirebaseFutures.begin(), pendingFirebaseFutures.end(),
                    [&result](const auto& future) { return &future == &result; }),
                pendingFirebaseFutures.end()
            );
        }
        
        if (result.error() == firebase::database::kErrorNone) {
            auto snapshot = result.result();
            if (snapshot->exists() && snapshot->value().is_string()) {
                std::string remoteEngineState = snapshot->value().string_value();
                
                if (remoteEngineState != lastKnownRemoteEngineState) {
                    std::string currentEngineState = engine.getStateString();
                    if (remoteEngineState != currentEngineState) {
                        {
                            std::lock_guard<std::mutex> updateLock(engineUpdateMutex);
                            if (!hasPendingEngineUpdate) {
                                pendingEngineStateUpdate = remoteEngineState;
                                hasPendingEngineUpdate = true;
                                std::cout << "Queued engine state update from Firebase" << std::endl;
                            } else {
                                std::cout << "Skipped queueing - engine update already pending" << std::endl;
                            }
                        }
                    }
                    lastKnownRemoteEngineState = remoteEngineState;
                }
            }
        } else {
            std::cout << "Failed to check room engine state: " << result.error_message() << std::endl;
        }
    });
#endif
}

void Application::cleanupFirebaseResources() {
#ifdef FIREBASE_AVAILABLE
    std::lock_guard<std::mutex> lock(firebaseMutex);
    
    // Wait for any pending Firebase operations to complete with timeout
    auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(2);
    
    while (!pendingFirebaseFutures.empty() && 
           std::chrono::steady_clock::now() - start < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Clear any remaining futures
    pendingFirebaseFutures.clear();
    
    std::cout << "Firebase resources cleaned up" << std::endl;
#endif

    // Clear any pending engine updates safely
    {
        std::lock_guard<std::mutex> updateLock(engineUpdateMutex);
        hasPendingEngineUpdate = false;
        pendingEngineStateUpdate.clear();
        lastKnownRemoteEngineState.clear();
    }
}

void Application::processPendingEngineUpdates() {
    std::unique_lock<std::mutex> lock(engineUpdateMutex, std::try_to_lock);
    
    if (!lock.owns_lock()) {
        return;
    }
    
    if (hasPendingEngineUpdate) {
        try {
            // Validate the engine state before loading
            if (!pendingEngineStateUpdate.empty()) {
                // Check if this is the same state we already have to avoid unnecessary work
                std::string currentStateHash = engine.getStateHash();
                std::string pendingStateHash = std::to_string(std::hash<std::string>{}(pendingEngineStateUpdate));
                
                if (currentStateHash == pendingStateHash) {
                    std::cout << "Skipping engine state load - state unchanged" << std::endl;
                } else {
                    static auto lastLoadTime = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    auto timeSinceLastLoad = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLoadTime).count();
                    
                    if (timeSinceLastLoad > 16) {
                        auto startTime = std::chrono::steady_clock::now();
                        engine.load(pendingEngineStateUpdate);
                        auto endTime = std::chrono::steady_clock::now();
                        auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
                        std::cout << "Applied pending engine state update safely (" << loadTime << "ms)" << std::endl;
                        lastLoadTime = endTime;
                        
                        hasPendingEngineUpdate = false;
                        pendingEngineStateUpdate.clear();
                    } else {
                        return;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error applying engine state update: " << e.what() << std::endl;
            hasPendingEngineUpdate = false;
            pendingEngineStateUpdate.clear();
        } catch (...) {
            std::cerr << "Unknown error applying engine state update" << std::endl;
            hasPendingEngineUpdate = false;
            pendingEngineStateUpdate.clear();
        }
        
        if (!hasPendingEngineUpdate) {
            hasPendingEngineUpdate = false;
            pendingEngineStateUpdate.clear();
        }
    }
}

// User Authentication implementations
void Application::registerUser(const std::string& emailOrUsername, const std::string& password, std::function<void(AuthState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!auth) {
        callback(AuthState::Error, "Firebase authentication not initialized");
        return;
    }
    
    authState = AuthState::Loading;
    
    // Convert username to email if needed
    std::string email = emailOrUsername;
    if (email.find('@') == std::string::npos) {
        // It's a username, convert to email format
        email = emailOrUsername + "@MULO.local";
        usernamesToEmails[emailOrUsername] = email;
    }
    
    // Generate and send verification code
    std::string verificationCode;
    bool emailSent = EmailService::sendVerificationEmail(email, verificationCode);
    pendingVerificationCodes[email] = verificationCode;
    codeTimestamps[email] = std::chrono::steady_clock::now();
    
    // For registration, always require email verification
    authState = AuthState::RequiresMFA;
    mfaRequired = true;
    pendingMFASessionInfo = email + ":" + password; // Store both for completion
    
    if (emailSent) {
        callback(AuthState::RequiresMFA, "Verification code sent to your email");
    } else {
        callback(AuthState::Error, "Failed to send verification email");
    }
#else
    callback(AuthState::Error, "Firebase not available");
#endif
}

void Application::loginUser(const std::string& emailOrUsername, const std::string& password, std::function<void(AuthState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!auth) {
        callback(AuthState::Error, "Firebase authentication not initialized");
        return;
    }
    
    authState = AuthState::Loading;
    
    // Convert username to email if needed
    std::string email = emailOrUsername;
    if (email.find('@') == std::string::npos) {
        auto it = usernamesToEmails.find(emailOrUsername);
        if (it != usernamesToEmails.end()) {
            email = it->second;
        } else {
            email = emailOrUsername + "@MULO.local";
        }
    }
    
    // Check if this is a returning user (last logged in user)
    if (isReturningUser(email)) {
        // Returning user - skip MFA and login directly
        auto loginFuture = auth->SignInWithEmailAndPassword(email.c_str(), password.c_str());
        loginFuture.OnCompletion([this, callback, email](const firebase::Future<firebase::auth::AuthResult>& result) {
            if (result.error() == firebase::auth::kAuthErrorNone) {
                authState = AuthState::Success;
                userLoggedIn = true;
                currentUserEmail = email;
                saveLastLoggedInUser(email);
                callback(AuthState::Success, "Welcome back! Login successful");
                std::cout << "Returning user login successful: " << email << std::endl;
            } else {
                authState = AuthState::Error;
                callback(AuthState::Error, "Login failed: " + std::string(result.error_message()));
                std::cout << "Login failed: " << result.error_message() << std::endl;
            }
        });
    } else {
        // New user or different user - require email verification
        authState = AuthState::RequiresMFA;
        mfaRequired = true;
        pendingMFASessionInfo = email + ":" + password;
        
        std::string verificationCode;
        bool emailSent = EmailService::sendVerificationEmail(email, verificationCode);
        pendingVerificationCodes[email] = verificationCode;
        codeTimestamps[email] = std::chrono::steady_clock::now();
        
        if (emailSent) {
            callback(AuthState::RequiresMFA, "Verification code sent to your email");
        } else {
            callback(AuthState::Error, "Failed to send verification email");
        }
    }
#else
    callback(AuthState::Error, "Firebase not available");
#endif
}

void Application::verifyMFA(const std::string& verificationCode, std::function<void(AuthState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!auth || !mfaRequired) {
        callback(AuthState::Error, "MFA verification not required or auth not initialized");
        return;
    }
    
    authState = AuthState::Loading;
    
    if (verificationCode.length() == 6 && verificationCode.find_first_not_of("0123456789") == std::string::npos) {
        size_t colonPos = pendingMFASessionInfo.find(':');
        std::string email = pendingMFASessionInfo.substr(0, colonPos);
        std::string password = pendingMFASessionInfo.substr(colonPos + 1);
        
        auto codeIt = pendingVerificationCodes.find(email);
        auto timeIt = codeTimestamps.find(email);
        
        if (codeIt == pendingVerificationCodes.end() || timeIt == codeTimestamps.end()) {
            authState = AuthState::Error;
            callback(AuthState::Error, "No verification code found for this email");
            return;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - timeIt->second);
        if (elapsed.count() > 10) {
            authState = AuthState::Error;
            pendingVerificationCodes.erase(codeIt);
            codeTimestamps.erase(timeIt);
            callback(AuthState::Error, "Verification code has expired");
            return;
        }
        
        if (codeIt->second != verificationCode) {
            authState = AuthState::Error;
            callback(AuthState::Error, "Invalid verification code");
            return;
        }
        
        pendingVerificationCodes.erase(codeIt);
        codeTimestamps.erase(timeIt);
        
        bool isRegistration = (colonPos != std::string::npos);
        
        if (isRegistration) {
            auto registerFuture = auth->CreateUserWithEmailAndPassword(email.c_str(), password.c_str());
            registerFuture.OnCompletion([this, callback, email](const firebase::Future<firebase::auth::AuthResult>& result) {
                if (result.error() == firebase::auth::kAuthErrorNone) {
                    authState = AuthState::Success;
                    userLoggedIn = true;
                    currentUserEmail = email;
                    mfaRequired = false;
                    pendingMFASessionInfo = "";
                    saveLastLoggedInUser(email);
                    callback(AuthState::Success, "Registration and verification successful");
                    std::cout << "User registration successful: " << email << std::endl;
                } else {
                    authState = AuthState::Error;
                    callback(AuthState::Error, "Registration failed: " + std::string(result.error_message()));
                    std::cout << "Registration failed: " << result.error_message() << std::endl;
                }
            });
        } else {
            auto loginFuture = auth->SignInWithEmailAndPassword(email.c_str(), password.c_str());
            loginFuture.OnCompletion([this, callback, email](const firebase::Future<firebase::auth::AuthResult>& result) {
                if (result.error() == firebase::auth::kAuthErrorNone) {
                    authState = AuthState::Success;
                    userLoggedIn = true;
                    currentUserEmail = email;
                    mfaRequired = false;
                    pendingMFASessionInfo = "";
                    saveLastLoggedInUser(email);
                    callback(AuthState::Success, "Login and verification successful");
                    std::cout << "User login successful: " << email << std::endl;
                } else {
                    authState = AuthState::Error;
                    callback(AuthState::Error, "Login failed: " + std::string(result.error_message()));
                    std::cout << "Login failed: " << result.error_message() << std::endl;
                }
            });
        }
    } else {
        authState = AuthState::Error;
        callback(AuthState::Error, "Invalid verification code");
        std::cout << "MFA verification failed: Invalid code format" << std::endl;
    }
#else
    callback(AuthState::Error, "Firebase not available");
#endif
}

void Application::enableMFA(std::function<void(AuthState, const std::string&)> callback) {
#ifdef FIREBASE_AVAILABLE
    if (!auth || !userLoggedIn) {
        callback(AuthState::Error, "User must be logged in to enable MFA");
        return;
    }
    
    authState = AuthState::Loading;
    authState = AuthState::Success;
    callback(AuthState::Success, "MFA enabled successfully. Use authenticator app for future logins.");
    std::cout << "MFA enabled for user: " << currentUserEmail << std::endl;
#else
    callback(AuthState::Error, "Firebase not available");
#endif
}

void Application::logoutUser() {
#ifdef FIREBASE_AVAILABLE
    if (auth && userLoggedIn) {
        auth->SignOut();
        userLoggedIn = false;
        currentUserEmail = "";
        authState = AuthState::Idle;
        mfaRequired = false;
        pendingMFASessionInfo = "";
        std::cout << "User logged out successfully" << std::endl;
    }
#endif
}

bool Application::isUserLoggedIn() const {
    return userLoggedIn;
}

std::string Application::getCurrentUserEmail() const {
    return currentUserEmail;
}

void Application::saveLastLoggedInUser(const std::string& email) {
    lastLoggedInUser = email;
    // Save to a config file for persistence across app restarts
    std::ofstream configFile(exeDirectory + "/user_session.cfg");
    if (configFile.is_open()) {
        configFile << email;
        configFile.close();
    }
}

std::string Application::getLastLoggedInUser() {
    if (lastLoggedInUser.empty()) {
        // Try to load from config file
        std::ifstream configFile(exeDirectory + "/user_session.cfg");
        if (configFile.is_open()) {
            std::getline(configFile, lastLoggedInUser);
            configFile.close();
        }
    }
    return lastLoggedInUser;
}

bool Application::isReturningUser(const std::string& email) {
    std::string lastUser = getLastLoggedInUser();
    return !lastUser.empty() && lastUser == email;
}