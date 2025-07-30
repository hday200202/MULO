#include "Application.hpp"

Application::Application() {
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
    // Clean up MULOComponents
    if (baseContainer)
        baseContainer->m_markedForDeletion = true;
    
    cleanupMarkedElements();
    
    muloComponents.clear();
}

void Application::update() {
    running = ui->isRunning();
    shouldForceUpdate = forceUpdatePoll;

    if (ui->getVerticalScrollDelta() != 0 || ui->getHorizontalScrollDelta() != 0)
        shouldForceUpdate = true;
    
    // handle all input, determines if shouldForceUpdate
    handleEvents();

    bool rClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);
    bool lClick = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

    if (lClick || rClick) shouldForceUpdate = true;

    // (shouldForceUpdate) ? ui->forceUpdate(windowView) : ui->update(windowView);
    ui->forceUpdate(windowView);

    // if (shouldForceUpdate)
    for (const auto& [name, component] : muloComponents)
        if (component)
            component->update();

    if (forceUpdatePoll > 0) --forceUpdatePoll;
    
    freshRebuild = false;
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
    juce::File fontFile = juce::File::getCurrentWorkingDirectory()
        .getChildFile("assets/fonts/DejaVuSans.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/DejaVuSans.ttf");
    }
    resources.dejavuSansFont = fontFile.getFullPathName().toStdString();

    fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/SpaceMono-Regular.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/SpaceMono-Regular.ttf");
    }
    resources.spaceMonoFont = fontFile.getFullPathName().toStdString();

    fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/ubuntu.bold.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/ubuntu.bold.ttf");
    }
    resources.ubuntuBoldFont = fontFile.getFullPathName().toStdString();

    fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/ubuntu.mono.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/ubuntu.mono.ttf");
    }
    resources.ubuntuMonoFont = fontFile.getFullPathName().toStdString();

    fontFile = juce::File::getCurrentWorkingDirectory().getChildFile("assets/fonts/ubuntu.mono-bold.ttf");
    if (!fontFile.existsAsFile()) {
        fontFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("assets/fonts/ubuntu.mono-bold.ttf");
    }
    resources.ubuntuMonoBoldFont = fontFile.getFullPathName().toStdString();
}

std::string Application::selectDirectory() {
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        std::cerr << "FileChooser must be called from the message thread" << std::endl;
        return "";
    }
    
    juce::FileChooser chooser("Select directory", juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*");
    
    if (chooser.browseForDirectory()) {
        juce::File result = chooser.getResult();
        if (result.exists()) {
            return result.getFullPathName().toStdString();
        }
    }
    
    return "";
}

std::string Application::selectFile(std::initializer_list<std::string> filters) {
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        std::cerr << "FileChooser must be called from the message thread" << std::endl;
        return "";
    }
    
    juce::String filterString;
    for (auto it = filters.begin(); it != filters.end(); ++it) {
        if (it != filters.begin())
            filterString += ";";
        filterString += juce::String((*it).c_str());
    }

    std::cout << "Filter string: " << filterString << std::endl;

    juce::FileChooser chooser("Select audio file", juce::File(), filterString);
    if (chooser.browseForFileToOpen()) {
        return chooser.getResult().getFullPathName().toStdString();
    }
    return "";
}

void Application::createWindow() {
    screenResolution = sf::VideoMode::getDesktopMode();
    screenResolution.size.x /= 1.5f;
    screenResolution.size.y /= 1.5f;

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
}

void Application::loadComponents() {
    muloComponents["keyboard_shortcuts"]    = std::make_unique<KBShortcuts>();
    muloComponents["app_controls"]          = std::make_unique<AppControls>();
    muloComponents["timeline"]              = std::make_unique<TimelineComponent>();
    muloComponents["file_browser"]          = std::make_unique<FileBrowserComponent>();
    muloComponents["settings"]              = std::make_unique<SettingsComponent>();
    muloComponents["mixer"]                 = std::make_unique<MixerComponent>();

    // Load MULO Components
    for (auto& [name, component] : muloComponents) {
        if (!component) {
             std::cerr << "Error: Null component created for key: " << name << std::endl;
             continue;
        }
        component->setAppRef(this);
        component->setEngineRef(&engine);
        component->setResourcesRef(&resources);
        component->setUIStateRef(&uiState);
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
    // Clear everything
    applyTheme(resources, uiState.selectedTheme);

    cleanup();
    initUIResources();
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