#pragma once

#include "MULOComponent.hpp"

class AppControls : public MULOComponent {
public:
    AppControls();
    ~AppControls();

    void init() override;
    bool handleEvents() override;
    inline void update() override {}

private:
    Button* loadButton;
    Button* saveButton;
    Button* playButton;
    Button* mixerButton;
    Button* settingsButton;
    
    // Track play state to avoid reading button text
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
    
    loadButton = button(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setHeight(.75f)
            .setfixedWidth(96)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                std::string path = app->selectFile({"*.mpf"});
                if (!path.empty())
                    app->loadComposition(path);
            }),
        ButtonStyle::Pill, 
        "load", 
        app->resources.dejavuSansFont, 
        app->resources.activeTheme->secondary_text_color,
        "load"
    );

    saveButton = button(
        Modifier()
            .align(Align::LEFT | Align::CENTER_Y)
            .setfixedWidth(96).setHeight(.75f)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                app->uiState.saveDirectory = app->selectDirectory();
                if (app->saveState(app->uiState.saveDirectory + "/" + app->getCurrentCompositionName() + ".mpf"))
                    std::cout << "Project saved successfully." << std::endl;
                else
                    std::cerr << "Failed to save project." << std::endl;
            }),
        ButtonStyle::Pill,
        "save",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "save"
    );

    playButton = button(
        Modifier()
            .align(Align::CENTER_X | Align::CENTER_Y)
            .setfixedWidth(96).setHeight(.75f)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                if (!app->isPlaying()) {
                    app->play();
                } else {
                    app->pause();
                    app->setPosition(0.0);
                }
                app->shouldForceUpdate = true;
            }),
        ButtonStyle::Pill,
        "play",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "play"
    );

    settingsButton = button(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setHeight(.75f)
            .setfixedWidth(96)
            .setColor(app->resources.activeTheme->button_color)
            .onLClick([&](){
                app->uiState.settingsShown = !app->uiState.settingsShown;
                std::cout << (app->uiState.settingsShown ? "Show Settings" : "Hide Settings") << std::endl;
            }),
        ButtonStyle::Pill,
        "settings",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "settings"
    );

    mixerButton = button(
        Modifier()
            .align(Align::RIGHT | Align::CENTER_Y)
            .setfixedWidth(96).setHeight(.75f)
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
        ButtonStyle::Pill,
        "mixer",
        app->resources.dejavuSansFont,
        app->resources.activeTheme->secondary_text_color,
        "mixer"
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
            playButton,
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            settingsButton,
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            mixerButton,
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

    // Check if play state changed and update button text safely
    bool currentlyPlaying = app->isPlaying();
    if (currentlyPlaying != wasPlaying) {
        // Only try to update text if we have a valid playButton pointer
        if (playButton) {
            try {
                // Use setText which is generally safer than getText
                if (currentlyPlaying) {
                    playButton->setText("pause");
                } else {
                    playButton->setText("play");
                }
                forceUpdate = true;
            } catch (...) {
                // If setText fails, the button might be invalid - just ignore
            }
        }
        wasPlaying = currentlyPlaying;
    }
    
    return forceUpdate;
}

// Plugin interface for AppControls
extern "C" {
    __attribute__((visibility("default"))) PluginVTable* getPluginInterface();
}

// Declare this class as a plugin
DECLARE_PLUGIN(AppControls)