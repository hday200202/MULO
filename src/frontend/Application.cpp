

#include "Application.hpp"
#include "Util/PlatformUtils.hpp"
#include "Data/Resources.hpp"
#include "../audio/MIDIClip.hpp"

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef Success
#undef Success
#endif

// used to prevent exit on x11 errors (fixes fullscreen toggle)
static XErrorHandler g_previousX11ErrorHandler = nullptr;
static int x11ErrorHandler(Display* display, XErrorEvent* event) {
    char errorText[256];
    XGetErrorText(display, event->error_code, errorText, sizeof(errorText));
    std::cerr << "X11 Error: " << errorText
              << " (request code: " << (int)event->request_code
              << ", minor code: " << (int)event->minor_code << ")" << std::endl;
    return 0;
}
#endif

#include <tinyfiledialogs/tinyfiledialogs.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <unordered_map>

namespace fs = std::filesystem;

Application::Application() {
}

void Application::initialise(const juce::String& commandLine) {
    logoPageTimer.restart();
#ifdef __linux__
    g_previousX11ErrorHandler = XSetErrorHandler(x11ErrorHandler);
#endif
    exeDirectory = PlatformUtils::getExecutableDirectory();
    
    loadConfig();
    if (!uiState.vstDirecory.empty())
        engine.setVSTDirectory(uiState.vstDirecory);
    if (!uiState.saveDirectory.empty())
        engine.setSampleDirectory(uiState.saveDirectory);
    else if (!uiState.fileBrowserDirectory.empty())
        engine.setSampleDirectory(uiState.fileBrowserDirectory);
    
    createWindow();
    applyTheme(resources, uiState.selectedTheme);
    Resources::initUIResources(resources, exeDirectory);
    initUI();
    
    globalSettings = std::make_unique<GlobalSettings>(*this);
    
    // Register Application settings section
    globalSettings->addSection("Application", {
        textboxSetting("Composition Name", &uiState.compositionName, getCurrentCompositionName(), 256, [this]() {
            engine.setCurrentCompositionName(uiState.compositionName);
        }),
        textboxSetting("BPM", &uiState.bpmStr, std::to_string(static_cast<int>(getBpm())), 8, [this]() {
            try {
                float bpm = std::stof(uiState.bpmStr);
                if (bpm >= 20.0f && bpm <= 999.0f) engine.setBpm(bpm);
            } catch (...) {}
        }),
        dropdownSetting("Sample Rate", &uiState.sampleRateStr, {"44100", "48000", "96000"}, 
            std::to_string(static_cast<int>(engine.getSampleRate())), [this]() {
            try {
                double sampleRate = std::stod(uiState.sampleRateStr);
                setSampleRate(sampleRate);
            } catch (...) {}
        }),
        dropdownSetting("UI Theme", &uiState.selectedTheme, getThemeNames(), 
            uiState.selectedTheme, [this]() {
            writeConfig("selectedTheme", uiState.selectedTheme);
            requestThemeChange(uiState.selectedTheme);
            uiState.settingsShown = false;
            if (globalSettings) globalSettings->hideWindow();
        }),
        buttonSetting("Save Component Layout", "Save", [this](){ saveLayoutConfig(); })
    });

    engine.newComposition("untitled");
    engine.addTrack("Master");

    running = ui->isRunning();

    loadComponents();
    loadLayoutConfig();    
    initFirebase();

    createLogoScreen();

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
#ifdef __linux__
    // restore previous x11 handler if we set one
    if (g_previousX11ErrorHandler) {
        XSetErrorHandler(g_previousX11ErrorHandler);
        g_previousX11ErrorHandler = nullptr;
    }
#endif
}

void Application::shutdown() {
    unloadAllPlugins();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
#ifdef __linux__
    if (g_previousX11ErrorHandler) {
        XSetErrorHandler(g_previousX11ErrorHandler);
        g_previousX11ErrorHandler = nullptr;
    }
#endif
}

void Application::update() {
    using namespace sf::Keyboard;
    using namespace sf::Mouse;
    using mb = sf::Mouse::Button;
    using kb = sf::Keyboard::Key;
    
    running = ui->isRunning();
    if (!running) return;

    if (logoPageTimer.getElapsedTime().asMilliseconds() > 2000)
        if (currentPage == "logo_page") { ui->switchToPage("base"); currentPage = "base"; }
    
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
    
    if (globalSettings)
        globalSettings->update();
    
    handleColorPicker();
    
    // if (fileBrowserActive) {
    //     fileBrowser->update();

    //     if (!fileBrowser->isOpen()) {
    //         std::string selectedPath = fileBrowser->getSelectedPath();
    //         if (!selectedPath.empty()) {
    //             switch (pendingFileBrowserAction) {
    //                 case FileBrowserAction::EXPORT:
    //                     engine.exportMaster(selectedPath);
    //                     break;
    //                 default:
    //                     break;
    //             }
    //         }
    //         fileBrowserActive = false;
    //         pendingFileBrowserAction = FileBrowserAction::NONE;
    //     }
    // }

    // Handle cursor management after all components have updated
    if (uiState.xResizing) {
        sf::Cursor cursor(sf::Cursor::Type::SizeHorizontal);
        window.setMouseCursor(cursor);
    } else if (uiState.yResizing) {
        sf::Cursor cursor(sf::Cursor::Type::SizeVertical);
        window.setMouseCursor(cursor);
    } else {
        sf::Cursor cursor(sf::Cursor::Type::Arrow);
        window.setMouseCursor(cursor);
    }
    
    // Reset resize flags for next frame
    uiState.xResizing = false;
    uiState.yResizing = false;

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
    
    renderColorPicker();
}

void Application::handleEvents() {
    if (hasPendingThemeChange) {
        if (!engine.isPlaying()) {
            applyTheme(resources, pendingThemeName);
            pendingUIRebuild = true;
            hasPendingThemeChange = false;
            pendingThemeName.clear();
        }
    }

    for (auto& component : muloComponents)
        shouldForceUpdate |= component.second->handleEvents();

    if (pendingUIRebuild) {
        if (!engine.isPlaying()) {
            rebuildUI();
            loadLayoutConfig();
            pendingUIRebuild = false;
        }
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
                    
                    if (effect->isSynthesizer())
                        engine.sendBpmToSynthesizers();
                }

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
            }
        }
        hasPendingSynth = false;
        pendingSynthPath.clear();
    }

    if (hasPendingEffectWindow) {
        Track* selectedTrack = getSelectedTrackPtr();
        if (selectedTrack) {
            auto& effects = selectedTrack->getEffects();
            if (pendingEffectWindowIndex < effects.size())
                effects[pendingEffectWindowIndex]->openWindow();
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
        } else
            targetTrack = getTrack(deferredEffect.trackName);
        
        if (targetTrack) {
            Effect* effect = targetTrack->addEffect(deferredEffect.vstPath);
            if (effect) {
                if (!deferredEffect.enabled)
                    effect->disable();
                if (deferredEffect.index >= 0)
                    effect->setIndex(deferredEffect.index);
                
                for (const auto& paramPair : deferredEffect.parameters)
                    effect->setParameter(paramPair.first, paramPair.second);
                
                if (effect->isSynthesizer())
                    engine.sendBpmToSynthesizers();
                
                if (effect->hasEditor()) {
                    effect->openWindow();
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    effect->closeWindow();
                }
            }
        }
        
        if (deferredEffects.empty()) hasDeferredEffects = false;
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
    }

    if (pendingTrackRemoveName != "") {
        auto track = engine.getTrackByName(pendingTrackRemoveName);
        
        if (track) track->clearEffects();
            
        engine.removeTrackByName(pendingTrackRemoveName);
        pendingTrackRemoveName = "";
    }

    handleDragAndDrop();
}

void Application::requestThemeChange(const std::string& themeName) {
    pendingThemeName = themeName;
    hasPendingThemeChange = true;
}

void Application::openColorPicker(sf::Vector2f position) {
    closeColorPicker();
    
    std::vector<sf::Color> colors = {
        hexToColor("#dd6363ff"), hexToColor("#dd7c63ff"), hexToColor("#dd9463ff"), hexToColor("#ddae63ff"),
        hexToColor("#ddc763ff"), hexToColor("#98dd63ff"), hexToColor("#69dd63ff"), hexToColor("#63dd7cff"),
        hexToColor("#63dda2ff"), hexToColor("#63a2ddff"), hexToColor("#6382ddff"), hexToColor("#6963ddff"),
        hexToColor("#9863ddff"), hexToColor("#cd63ddff"), hexToColor("#dd63aeff"), hexToColor("#dd637dff"),
        hexToColor("#ffffffff"), hexToColor("#c0c0c0ff"), hexToColor("#8d8d8dff"), hexToColor("#525252ff"),
        hexToColor("#ffa0a0ff"), hexToColor("#ffd4a0ff"), hexToColor("#a0ffa0ff"), hexToColor("#00000000"),
    };

    const int columns = 4;
    const int rows = 6;
    const float cellSize = 48.f;
    const float padding = 10.f;
    
    sf::Vector2u windowSize(
        static_cast<unsigned int>(columns * cellSize),
        static_cast<unsigned int>(rows * cellSize)
    );
    
    colorPickerWindow.create(sf::VideoMode(windowSize), "", sf::Style::None);
    
    sf::Vector2i screenPos = window.getPosition();
    sf::Vector2f windowPos = position;
    colorPickerWindow.setPosition(sf::Vector2i(
        screenPos.x + static_cast<int>(windowPos.x),
        screenPos.y + static_cast<int>(windowPos.y)
    ));
    
    sf::View view;
    view.setSize(static_cast<sf::Vector2f>(windowSize));
    sf::Vector2f center(view.getSize().x / 2.0f, view.getSize().y / 2.0f);
    view.setCenter(center);
    colorPickerWindow.setView(view);
    
    colorPickerUI = std::make_unique<UILO>(colorPickerWindow, view);
    
    colorPickerGrid = grid(
        Modifier()
            .setColor(resources.activeTheme->foreground_color)
            .align(Align::CENTER_X | Align::CENTER_Y),
        cellSize,
        cellSize,
        columns,
        rows,
        contains{},
        "color_picker_grid"
    );
    
    for (size_t i = 0; i < colors.size(); i++) {
        sf::Color buttonColor = colors[i];
        ButtonStyle style = ButtonStyle::Pill;
        std::string buttonText = "";
        
        if (i == colors.size() - 1) {
            // For the transparent/"clear color" button, display it with middle_color background
            // but keep the actual stored color as transparent (alpha=0) for detection
            buttonText = "X";
        }
        
        auto* colorButton = button(
            Modifier()
                .setfixedWidth(cellSize)
                .setfixedHeight(cellSize)
                .setColor(i == colors.size() - 1 ? resources.activeTheme->middle_color : buttonColor)
                .align(Align::CENTER_X | Align::CENTER_Y),
            style,
            buttonText,
            "",
            sf::Color::White,
            "color_button_" + std::to_string(i)
        );
        colorPickerGrid->addElement(colorButton);
    }
    
    auto* container = uilo::row(
        Modifier()
            .align(Align::CENTER_X | Align::CENTER_Y),
        contains{colorPickerGrid},
        "color_picker_container"
    );
    
    colorPickerUI->addPage(page({container}), "color_picker");
    
    colorPickerWindow.setVisible(true);
    colorPickerWindow.requestFocus();
    
    ui->setInputBlocked(true);
    
    colorPickerOpen = true;
    colorWasSelected = false;
}

void Application::handleColorPicker() {
    if (!colorPickerOpen) return;

    if (!colorPickerWindow.hasFocus()) {
        closeColorPicker();
        return;
    }

    colorPickerUI->forceUpdate();
    
    if (!colorPickerUI->isRunning()) {
        closeColorPicker();
        return;
    }

    if (colorPickerGrid) {
        auto elements = colorPickerGrid->getElements();
        for (size_t i = 0; i < elements.size(); i++) {
            auto* button = dynamic_cast<uilo::Button*>(elements[i]);
            if (button && button->isClicked()) {
                // Last button is the "clear color" button (transparent)
                if (i == elements.size() - 1) {
                    recentColorPicked = sf::Color(0, 0, 0, 0);
                } else {
                    recentColorPicked = elements[i]->m_modifier.getColor();
                }
                colorWasSelected = true;
                closeColorPicker();
                return;
            }
        }
    }
}

void Application::renderColorPicker() {
    if (!colorPickerOpen) return;
    
    if (colorPickerUI->windowShouldUpdate()) {
        colorPickerWindow.clear(resources.activeTheme->foreground_color);
        colorPickerUI->render();
        colorPickerWindow.display();
    }
}

const sf::Color& Application::getRecentColorPicked() const {
    return recentColorPicked;
}

bool Application::isColorPickerOpen() const {
    return colorPickerOpen;
}

bool Application::wasColorSelected() const {
    return colorWasSelected;
}

void Application::closeColorPicker() {
    if (colorPickerOpen) {
        colorPickerWindow.close();
        colorPickerUI.reset();
        colorPickerGrid = nullptr;
        ui->setInputBlocked(false);
        colorPickerOpen = false;
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

    // Set minimum window size using platform utilities
    PlatformUtils::setMinimumWindowSize(window, minWindowSize.x, minWindowSize.y);
}

void Application::rebuildUI() {
    unloadAllPlugins();
    muloComponents.clear();

    cleanup();
    initUI();
    createLogoScreen();
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

void Application::createLogoScreen() {
    if (!ui) return;

    auto muloLogo = image(
        Modifier()
            .setColor(resources.activeTheme->button_color)
            .setfixedHeight(320.f)
            .setfixedWidth(320.f)
            .align(Align::CENTER_X | Align::CENTER_Y),
        resources.muloIcon,
        true,
        "mulo_init_screen_icon"
    );

    ui->addPage(page({
        column(
            Modifier().setColor(resources.activeTheme->middle_color),
        contains{
            muloLogo
        })
    }), "logo_page");

    ui->switchToPage("logo_page");
    currentPage = "logo_page";
    logoPageTimer.restart();
}
