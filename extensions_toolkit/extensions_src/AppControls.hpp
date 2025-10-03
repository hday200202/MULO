
#pragma once

#include "MULOComponent.hpp"
#include "../../src/DebugConfig.hpp"

class AppControls : public MULOComponent {
public:
    AppControls();
    ~AppControls();
    
    void init() override;
    bool handleEvents() override;
    inline void update() override {}

private:
    Image* loadButton;
    Image* saveButton;
    Image* exportButton;
    Image* playButton;
    Image* metronomeButton;
    Image* automationButton;
    Image* pianoRollButton;
    Image* mixerButton;
    Image* extStore;
    Image* settingsButton;

    bool wasPlaying = false;
};

#include "Application.hpp"

AppControls::AppControls() {
    name = "app_controls";
}

AppControls::~AppControls() {
    
}

void AppControls::init() {
    if (app->baseContainer)
        parentContainer = app->baseContainer;
    
    loadButton = image(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                std::string path = app->selectFile({"*.mpf"});
                if (!path.empty())
                    app->loadComposition(path);
            }),
        app->resources.loadIcon,
        true,
        "load_button"
    );

    saveButton = image(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48.f)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                app->uiState.saveDirectory = app->selectDirectory();
                std::string savePath = app->uiState.saveDirectory + "/" + app->getCurrentCompositionName() + ".mpf";
                app->saveState();
                app->saveToFile(savePath);
                DEBUG_PRINT("Project saved successfully to: " << savePath);
            }),
        app->resources.saveIcon,
        true,
        "save_button"
    );

    exportButton = image(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48.f)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                DEBUG_PRINT("Exporting Master...");
                app->exportAudio();
            }),
        app->resources.exportIcon,
        true,
        "export_button"
    );

    playButton = image(
        Modifier()
            .align(Align::CENTER_X | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                if (!app->isPlaying()) {
                    app->play();
                } else {
                    app->pause();
                    app->setPosition(app->getSavedPosition());
                }
                app->shouldForceUpdate = true;
            }),
        app->resources.playIcon,
        true,
        "play_button"
    );

    metronomeButton = image(
        Modifier()
            .align(Align::CENTER_X | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48.f)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                app->setMetronomeEnabled(!app->isMetronomeEnabled());
            }),
        app->resources.metronomeIcon,
        true,
        "metronome_button"
    );

    automationButton = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                app->writeConfig<bool>("show_automation", 
                    !app->readConfig<bool>("show_automation", false));
            }),
        app->resources.automationIcon,
        true,
        "show_automation_button"
    );

    pianoRollButton = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                auto* pianoRoll = app->getComponent("piano_roll");
                if (pianoRoll) {
                    if (pianoRoll->isVisible()) {
                        pianoRoll->hide();
                    } else {
                        pianoRoll->show();
                    }
                } else {
                    DEBUG_PRINT("Piano Roll component not found!");
                }
            }),
        app->resources.pianoRollIcon,
        true,
        "piano_roll_button"
    );

    extStore = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                app->uiState.marketplaceShown = !app->uiState.marketplaceShown;
                DEBUG_PRINT((app->uiState.marketplaceShown ? "Show Marketplace" : "Hide Marketplace"));
            }),
        app->resources.storeIcon,
        true,
        "store_button"
    );

    settingsButton = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                app->uiState.settingsShown = !app->uiState.settingsShown;
                DEBUG_PRINT((app->uiState.settingsShown ? "Show Settings" : "Hide Settings"));
            }),
        app->resources.settingsIcon,
        true,
        "settings_button"
    );

    mixerButton = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48.f)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                if (app) {
                    if (app->getComponent("mixer")) {
                        auto* mixer = app->getComponent("mixer");
                        if (mixer) {
                            if (mixer->isVisible())
                                mixer->hide();
                            else
                                mixer->show();
                        }
                        app->shouldForceUpdate = true;
                    }
                }
            }),
        app->resources.mixerIcon,
        true,
        "mixer_button"
    );

    layout = row(
        Modifier()
            .setWidth(1.f)
            .setfixedHeight(64)
            .setColor(app->resources.activeTheme->foreground_color)
            .align(Align::TOP | Align::LEFT),
        contains{
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            loadButton,
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            saveButton,
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            exportButton,
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            playButton,
            spacer(Modifier().setfixedWidth(16).align(Align::CENTER_X)),
            metronomeButton,
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            automationButton,
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            pianoRollButton,
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            mixerButton,
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            extStore,
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            settingsButton,
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
        }
    );

    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

bool AppControls::handleEvents() { 
    bool forceUpdate = false;

    // Capture base colors at the start
    sf::Color baseButtonColor = app->resources.activeTheme->button_color;
    sf::Color baseMuteColor = app->resources.activeTheme->mute_color;
    
    sf::Color playButtonBaseColor = app->isPlaying() ? baseMuteColor : baseButtonColor;
    sf::Color metronomeButtonBaseColor = app->isMetronomeEnabled() ? baseMuteColor : baseButtonColor;
    sf::Color settingsButtonBaseColor = app->uiState.settingsShown ? baseMuteColor : baseButtonColor;
    sf::Color extStoreBaseColor = app->uiState.marketplaceShown ? baseMuteColor : baseButtonColor;

    bool currentlyPlaying = app->isPlaying();
    if (currentlyPlaying != wasPlaying) {
        if (playButton) {
            try {
                if (currentlyPlaying) {
                    playButton->m_modifier.setColor(baseMuteColor);
                    playButton->setImage(app->resources.pauseIcon, true);
                } else {
                    playButton->m_modifier.setColor(baseButtonColor);
                    playButton->setImage(app->resources.playIcon, true);
                }
                forceUpdate = true;
            } catch (...) {
            }
        }
        wasPlaying = currentlyPlaying;
    }

    if (app->isMetronomeEnabled()) {
        metronomeButton->m_modifier.setColor(baseMuteColor);
        metronomeButton->setImage(app->resources.metronomeIcon, true);
    }
    else {
        metronomeButton->m_modifier.setColor(baseButtonColor);
        metronomeButton->setImage(app->resources.metronomeIcon, true);
    }

    if (app->uiState.settingsShown) {
        settingsButton->m_modifier.setColor(baseMuteColor);
        settingsButton->setImage(app->resources.settingsIcon, true);
    }
    else {
        settingsButton->m_modifier.setColor(baseButtonColor);
        settingsButton->setImage(app->resources.settingsIcon, true);
    }

    if (app->uiState.marketplaceShown) {
        extStore->m_modifier.setColor(baseMuteColor);
        extStore->setImage(app->resources.storeIcon, true);
    }
    else {
        extStore->m_modifier.setColor(baseButtonColor);
        extStore->setImage(app->resources.storeIcon, true);
    }

    // Apply hover effects using the captured base colors
    if (loadButton->isHovered()) {
        loadButton->m_modifier.setColor(sf::Color(
            std::min(255, (int)(baseButtonColor.r + 50)),
            std::min(255, (int)(baseButtonColor.g + 50)),
            std::min(255, (int)(baseButtonColor.b + 50))
        ));
        loadButton->setImage(app->resources.loadIcon, true);
        loadButton->m_isHovered = false;
    }
    else {
        loadButton->m_modifier.setColor(baseButtonColor);
        loadButton->setImage(app->resources.loadIcon, true);
    }

    if (saveButton->isHovered()) {
        saveButton->m_modifier.setColor(sf::Color(
            std::min(255, (int)(baseButtonColor.r + 50)),
            std::min(255, (int)(baseButtonColor.g + 50)),
            std::min(255, (int)(baseButtonColor.b + 50))
        ));
        saveButton->setImage(app->resources.saveIcon, true);
        saveButton->m_isHovered = false;
    }
    else {
        saveButton->m_modifier.setColor(baseButtonColor);
        saveButton->setImage(app->resources.saveIcon, true);
    }

    if (exportButton->isHovered()) {
        exportButton->m_modifier.setColor(sf::Color(
            std::min(255, (int)(baseButtonColor.r + 50)),
            std::min(255, (int)(baseButtonColor.g + 50)),
            std::min(255, (int)(baseButtonColor.b + 50))
        ));
        exportButton->setImage(app->resources.exportIcon, true);
        exportButton->m_isHovered = false;
    }
    else {
        exportButton->m_modifier.setColor(baseButtonColor);
        exportButton->setImage(app->resources.exportIcon, true);
    }

    if (playButton->isHovered()) {
        playButton->m_modifier.setColor(sf::Color(
            std::min(255, (int)(playButtonBaseColor.r + 50)),
            std::min(255, (int)(playButtonBaseColor.g + 50)),
            std::min(255, (int)(playButtonBaseColor.b + 50))
        ));
        playButton->setImage(currentlyPlaying ? app->resources.pauseIcon : app->resources.playIcon, true);
        playButton->m_isHovered = false;
    }
    else {
        playButton->m_modifier.setColor(playButtonBaseColor);
        playButton->setImage(currentlyPlaying ? app->resources.pauseIcon : app->resources.playIcon, true);
    }

    if (metronomeButton->isHovered()) {
        metronomeButton->m_modifier.setColor(sf::Color(
            std::min(255, (int)(metronomeButtonBaseColor.r + 50)),
            std::min(255, (int)(metronomeButtonBaseColor.g + 50)),
            std::min(255, (int)(metronomeButtonBaseColor.b + 50))
        ));
        metronomeButton->setImage(app->resources.metronomeIcon, true);
        metronomeButton->m_isHovered = false;
    }

    if (automationButton->isHovered()) {
        automationButton->m_modifier.setColor(sf::Color(
            std::min(255, (int)(baseButtonColor.r + 50)),
            std::min(255, (int)(baseButtonColor.g + 50)),
            std::min(255, (int)(baseButtonColor.b + 50))
        ));
        automationButton->setImage(app->resources.automationIcon, true);
        automationButton->m_isHovered = false;
    }
    else {
        automationButton->m_modifier.setColor(baseButtonColor);
        automationButton->setImage(app->resources.automationIcon, true);
    }

    if (pianoRollButton->isHovered()) {
        pianoRollButton->m_modifier.setColor(sf::Color(
            std::min(255, (int)(baseButtonColor.r + 50)),
            std::min(255, (int)(baseButtonColor.g + 50)),
            std::min(255, (int)(baseButtonColor.b + 50))
        ));
        pianoRollButton->setImage(app->resources.pianoRollIcon, true);
        pianoRollButton->m_isHovered = false;
    }
    else {
        pianoRollButton->m_modifier.setColor(baseButtonColor);
        pianoRollButton->setImage(app->resources.pianoRollIcon, true);
    }

    if (extStore->isHovered()) {
        extStore->m_modifier.setColor(sf::Color(
            std::min(255, (int)(extStoreBaseColor.r + 50)),
            std::min(255, (int)(extStoreBaseColor.g + 50)),
            std::min(255, (int)(extStoreBaseColor.b + 50))
        ));
        extStore->setImage(app->resources.storeIcon, true);
        extStore->m_isHovered = false;
    }

    if (settingsButton->isHovered()) {
        settingsButton->m_modifier.setColor(sf::Color(
            std::min(255, (int)(settingsButtonBaseColor.r + 50)),
            std::min(255, (int)(settingsButtonBaseColor.g + 50)),
            std::min(255, (int)(settingsButtonBaseColor.b + 50))
        ));
        settingsButton->setImage(app->resources.settingsIcon, true);
        settingsButton->m_isHovered = false;
    }

    if (mixerButton->isHovered()) {
        mixerButton->m_modifier.setColor(sf::Color(
            std::min(255, (int)(baseButtonColor.r + 50)),
            std::min(255, (int)(baseButtonColor.g + 50)),
            std::min(255, (int)(baseButtonColor.b + 50))
        ));
        mixerButton->setImage(app->resources.mixerIcon, true);
        mixerButton->m_isHovered = false;
    }
    else {
        mixerButton->m_modifier.setColor(baseButtonColor);
        mixerButton->setImage(app->resources.mixerIcon, true);
    }

    return forceUpdate;
}


GET_INTERFACE
DECLARE_PLUGIN(AppControls)