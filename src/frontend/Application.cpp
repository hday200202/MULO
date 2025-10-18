

#include "Application.hpp"
#include "Util/PlatformUtils.hpp"
#include "Data/Resources.hpp"
#include "../audio/MIDIClip.hpp"

#include <tinyfiledialogs/tinyfiledialogs.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <unordered_map>

namespace fs = std::filesystem;

Application::Application() {}

void Application::initialise(const juce::String& commandLine) {
    exeDirectory = PlatformUtils::getExecutableDirectory();
    
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
    Resources::initUIResources(resources, exeDirectory);
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
