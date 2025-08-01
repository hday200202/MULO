#include "Application.hpp"

#include <tinyfiledialogs/tinyfiledialogs.hpp>
#include <filesystem>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#ifdef _WIN32
#include <windows.h>

static POINT s_minWindowSize = {800, 600}; // Set this to your minWindowSize

LRESULT CALLBACK MinSizeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_GETMINMAXINFO) {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = s_minWindowSize.x;
        mmi->ptMinTrackSize.y = s_minWindowSize.y;
        return 0;
    }
    // Call the original window proc
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

Application::Application() {
    exeDirectory = fs::canonical("/proc/self/exe").parent_path().string();
    
    // Initialize window, engine, ui
    createWindow();
    applyTheme(resources, uiState.selectedTheme);
    initUIResources();
    initUI();

    engine.newComposition("untitled");
    engine.addTrack("Master");

    running = ui->isRunning();

    loadComponents();

    ui->forceUpdate();
}

Application::~Application() {
    // Unload all plugins before destroying the application
    unloadAllPlugins();
    
    // Give some time for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Destroy JUCE components and handle any cleanup here
}

void Application::update() {
    using namespace sf::Keyboard;
    using namespace sf::Mouse;
    using mb = sf::Mouse::Button;
    using kb = sf::Keyboard::Key;
    
    running = ui->isRunning();
    shouldForceUpdate = forceUpdatePoll;

    if (ui->getVerticalScrollDelta() != 0 || ui->getHorizontalScrollDelta() != 0)
        shouldForceUpdate = true;
    
    // handle all input, determines if shouldForceUpdate
    handleEvents();

    bool rClick = isButtonPressed(mb::Right);
    bool lClick = isButtonPressed(mb::Left);
    bool ctrlShftR = isKeyPressed(kb::LControl) && isKeyPressed(kb::LShift) && isKeyPressed(kb::R);

    if (lClick || rClick) shouldForceUpdate = true;
    if (ctrlShftR && !prevCtrlShftR) {
        std::cout << "Ctrl+Shift+R\n";
        rebuildUI();
    }

    // (shouldForceUpdate) ? ui->forceUpdate(windowView) : ui->update(windowView);
    ui->forceUpdate(windowView);

    // if (shouldForceUpdate)
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
        window.display();
    }
}

void Application::handleEvents() {
    // handle events of all muloComponents
    // they will set shouldForceUpdate
    // through their reference to application
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

        // Not found
        return "";
    };

    resources.dejavuSansFont    = findFont("DejaVuSans.ttf");
    resources.spaceMonoFont     = findFont("SpaceMono-Regular.ttf");
    resources.ubuntuBoldFont    = findFont("ubuntu.bold.ttf");
    resources.ubuntuMonoFont    = findFont("ubuntu.mono.ttf");
    resources.ubuntuMonoBoldFont= findFont("ubuntu.mono-bold.ttf");
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
    settings.antiAliasingLevel = 8;

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

    #ifdef _WIN32
    HWND hwnd = (HWND)window.getSystemHandle();
    SetMinWindowSize(hwnd, minWindowSize.x, minWindowSize.y);
    #endif

    #ifdef __APPLE__
    void* nsWindow = window.getSystemHandle();
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

    if (!allInitialized) {
        std::cout << "Couldn't Initialize Components: \n";

        for (auto& [name, component] : muloComponents)
            if (!component->isInitialized())
                std::cout << "\t" + name + "\n";
    }
}

void Application::rebuildUI() {
    unloadAllPlugins();
    // Clear everything
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
            std::cout << "Found plugin: " << pluginPath << std::endl;
            if (loadPlugin(pluginPath)) {
                std::cout << "Successfully loaded plugin: " << pluginPath << std::endl;
            } else {
                std::cerr << "Failed to load plugin: " << pluginPath << std::endl;
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
            std::cout << "Plugin with name '" << name << "' already loaded, skipping" << std::endl;
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
        // Call destroy if available - this should clean up all JUCE objects
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
