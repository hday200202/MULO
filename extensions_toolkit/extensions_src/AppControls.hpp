
#pragma once

#include "Extension/MULOComponent.hpp"
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
    Image* loginButton;
    Image* playButton;
    Image* metronomeButton;
    Image* automationButton;
    Image* pianoRollButton;
    Image* mixerButton;
    Image* extStore;
    Image* settingsButton;
    Image* collaborationButton;
    Image* extensionUploaderButton;

    bool wasPlaying = false;
    
    struct ButtonImageStates {
        sf::Image normal;
        sf::Image hovered;
        sf::Image toggled;
        sf::Image toggledHovered;
    };
    
    ButtonImageStates loadButtonImages;
    ButtonImageStates saveButtonImages;
    ButtonImageStates exportButtonImages;
    ButtonImageStates loginButtonImages;
    ButtonImageStates playButtonImages;
    ButtonImageStates pauseButtonImages;
    ButtonImageStates metronomeButtonImages;
    ButtonImageStates automationButtonImages;
    ButtonImageStates pianoRollButtonImages;
    ButtonImageStates mixerButtonImages;
    ButtonImageStates extStoreImages;
    ButtonImageStates settingsButtonImages;
    ButtonImageStates collaborationButtonImages;
    ButtonImageStates extensionUploaderButtonImages;
    
    sf::Color baseButtonColor;
    sf::Color baseMuteColor;
    sf::Color hoverButtonColor;
    sf::Color hoverMuteColor;
    
    void generateButtonStates(ButtonImageStates& states, const sf::Image& sourceImage);
};

#include "Application.hpp"

AppControls::AppControls() {
    name = "app_controls";
}

AppControls::~AppControls() {
    
}

void AppControls::generateButtonStates(ButtonImageStates& states, const sf::Image& sourceImage) {
    auto recolorImage = [](const sf::Image& source, const sf::Color& color) -> sf::Image {
        sf::Image result = source;
        sf::Vector2u size = result.getSize();
        for (unsigned int y = 0; y < size.y; ++y) {
            for (unsigned int x = 0; x < size.x; ++x) {
                sf::Color pixel = result.getPixel({x, y});
                if (pixel.a > 0) {
                    result.setPixel({x, y}, sf::Color(color.r, color.g, color.b, pixel.a));
                }
            }
        }
        return result;
    };
    
    states.normal = recolorImage(sourceImage, baseButtonColor);
    states.hovered = recolorImage(sourceImage, hoverButtonColor);
    states.toggled = recolorImage(sourceImage, baseMuteColor);
    states.toggledHovered = recolorImage(sourceImage, hoverMuteColor);
}

void AppControls::init() {
    if (app->baseContainer)
        parentContainer = app->baseContainer;
    
    baseButtonColor = app->resources.activeTheme->button_color;
    baseMuteColor = app->resources.activeTheme->mute_color;
    hoverButtonColor = sf::Color(
        std::min(255, (int)(baseButtonColor.r + 50)),
        std::min(255, (int)(baseButtonColor.g + 50)),
        std::min(255, (int)(baseButtonColor.b + 50))
    );
    hoverMuteColor = sf::Color(
        std::min(255, (int)(baseMuteColor.r + 50)),
        std::min(255, (int)(baseMuteColor.g + 50)),
        std::min(255, (int)(baseMuteColor.b + 50))
    );
    
    generateButtonStates(loadButtonImages, app->resources.loadIcon);
    generateButtonStates(saveButtonImages, app->resources.saveIcon);
    generateButtonStates(exportButtonImages, app->resources.exportIcon);
    generateButtonStates(loginButtonImages, app->resources.loginIcon);
    generateButtonStates(playButtonImages, app->resources.playIcon);
    generateButtonStates(pauseButtonImages, app->resources.pauseIcon);
    generateButtonStates(metronomeButtonImages, app->resources.metronomeIcon);
    generateButtonStates(automationButtonImages, app->resources.automationIcon);
    generateButtonStates(pianoRollButtonImages, app->resources.pianoRollIcon);
    generateButtonStates(mixerButtonImages, app->resources.mixerIcon);
    generateButtonStates(extStoreImages, app->resources.storeIcon);
    generateButtonStates(settingsButtonImages, app->resources.settingsIcon);
    generateButtonStates(collaborationButtonImages, app->resources.collabIcon);
    generateButtonStates(extensionUploaderButtonImages, app->resources.exportIcon);
    
    loadButton = image(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(baseButtonColor)
            .onLClick([&](){
                std::string path = app->selectFile({"*.mpf"});
                if (!path.empty())
                    app->loadComposition(path);
            }),
        loadButtonImages.normal,
        false,
        "load_button"
    );

    saveButton = image(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48.f)
            .setColor(baseButtonColor)
            .onLClick([&](){
                app->uiState.saveDirectory = app->selectDirectory();
                std::string savePath = app->uiState.saveDirectory + "/" + app->getCurrentCompositionName() + ".mpf";
                app->saveState();
                app->saveToFile(savePath);
                DEBUG_PRINT("Project saved successfully to: " << savePath);
            }),
        saveButtonImages.normal,
        false,
        "save_button"
    );

    exportButton = image(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48.f)
            .setColor(baseButtonColor)
            .onLClick([&](){
                DEBUG_PRINT("Exporting Master...");
                app->exportAudio();
            }),
        exportButtonImages.normal,
        false,
        "export_button"
    );

    loginButton = image(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48.f)
            .setColor(baseButtonColor)
            .onLClick([&](){
                bool currentState = app->readConfig<bool>("show_user_login", false);
                app->writeConfig("show_user_login", !currentState);
                DEBUG_PRINT((!currentState ? "Show Login" : "Hide Login"));
            }),
        loginButtonImages.normal,
        false,
        "login_button"
    );

    playButton = image(
        Modifier()
            .align(Align::CENTER_X | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48)
            .setColor(baseButtonColor)
            .onLClick([&](){
                if (!app->isPlaying()) {
                    app->play();
                } else {
                    app->pause();
                    app->setPosition(app->getSavedPosition());
                }
                app->shouldForceUpdate = true;
            }),
        playButtonImages.normal,
        false,
        "play_button"
    );

    metronomeButton = image(
        Modifier()
            .align(Align::CENTER_X | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48.f)
            .setColor(baseButtonColor)
            .onLClick([&](){
                app->setMetronomeEnabled(!app->isMetronomeEnabled());
            }),
        metronomeButtonImages.normal,
        false,
        "metronome_button"
    );

    automationButton = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(baseButtonColor)
            .onLClick([&](){
                app->writeConfig<bool>("show_automation", 
                    !app->readConfig<bool>("show_automation", false));
            }),
        automationButtonImages.normal,
        false,
        "show_automation_button"
    );

    pianoRollButton = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(baseButtonColor)
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
        pianoRollButtonImages.normal,
        false,
        "piano_roll_button"
    );

    extStore = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(baseButtonColor)
            .onLClick([&](){
                app->uiState.marketplaceShown = !app->uiState.marketplaceShown;
                DEBUG_PRINT((app->uiState.marketplaceShown ? "Show Marketplace" : "Hide Marketplace"));
            }),
        extStoreImages.normal,
        false,
        "store_button"
    );

    settingsButton = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(baseButtonColor)
            .onLClick([&](){
                app->uiState.settingsShown = !app->uiState.settingsShown;
                DEBUG_PRINT((app->uiState.settingsShown ? "Show Settings" : "Hide Settings"));
            }),
        settingsButtonImages.normal,
        false,
        "settings_button"
    );
    
    collaborationButton = image(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(baseButtonColor)
            .onLClick([&](){
                bool currentState = app->readConfig<bool>("collabShowWindow", false);
                app->writeConfig("collabShowWindow", !currentState);
                DEBUG_PRINT((!currentState ? "Show Collaboration" : "Hide Collaboration"));
            }),
        collaborationButtonImages.normal,
        false,
        "collaboration_button"
    );

    extensionUploaderButton = image(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedHeight(48.f)
            .setfixedWidth(48)
            .setColor(baseButtonColor)
            .onLClick([&](){
                bool currentState = app->readConfig<bool>("extupload_shown", false);
                app->writeConfig("extupload_shown", !currentState);
                DEBUG_PRINT((!currentState ? "Show Extension Uploader" : "Hide Extension Uploader"));
            }),
        extensionUploaderButtonImages.normal,
        false,
        "extension_uploader_button"
    );

    mixerButton = image(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedWidth(48).setfixedHeight(48.f)
            .setColor(baseButtonColor)
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
        mixerButtonImages.normal,
        false,
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
            loginButton,
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            collaborationButton,
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            extensionUploaderButton,
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

    bool currentlyPlaying = app->isPlaying();
    bool metronomeEnabled = app->isMetronomeEnabled();
    bool settingsShown = app->uiState.settingsShown;
    bool collaborationShown = app->readConfig<bool>("collabShowWindow", false);
    bool extensionUploaderShown = app->readConfig<bool>("extupload_shown", false);
    bool marketplaceShown = app->uiState.marketplaceShown;
    bool loginShown = app->readConfig<bool>("show_user_login", false);
    bool automationShown = app->readConfig<bool>("show_automation", false);
    
    if (currentlyPlaying != wasPlaying) {
        if (playButton) {
            if (currentlyPlaying) {
                playButton->setImage(playButton->isHovered() ? pauseButtonImages.toggledHovered : pauseButtonImages.toggled, false);
            } else {
                playButton->setImage(playButton->isHovered() ? playButtonImages.hovered : playButtonImages.normal, false);
            }
            forceUpdate = true;
        }
        wasPlaying = currentlyPlaying;
    }
    
    auto updateButton = [](Image* btn, ButtonImageStates& states, bool isToggled) {
        if (!btn) return;
        bool hovered = btn->isHovered();
        sf::Image* targetImage = nullptr;
        
        if (isToggled) {
            targetImage = hovered ? &states.toggledHovered : &states.toggled;
        } else {
            targetImage = hovered ? &states.hovered : &states.normal;
        }
        
        btn->setImage(*targetImage, false);
        if (hovered) btn->m_isHovered = false;
    };

    updateButton(loadButton, loadButtonImages, false);
    updateButton(saveButton, saveButtonImages, false);
    updateButton(exportButton, exportButtonImages, false);
    updateButton(loginButton, loginButtonImages, loginShown);
    updateButton(playButton, currentlyPlaying ? pauseButtonImages : playButtonImages, currentlyPlaying);
    updateButton(metronomeButton, metronomeButtonImages, metronomeEnabled);
    updateButton(automationButton, automationButtonImages, automationShown);
    updateButton(pianoRollButton, pianoRollButtonImages, false);
    updateButton(mixerButton, mixerButtonImages, false);
    updateButton(extStore, extStoreImages, marketplaceShown);
    updateButton(settingsButton, settingsButtonImages, settingsShown);
    updateButton(collaborationButton, collaborationButtonImages, collaborationShown);
    updateButton(extensionUploaderButton, extensionUploaderButtonImages, extensionUploaderShown);

    return forceUpdate;
}


GET_INTERFACE
DECLARE_PLUGIN(AppControls)