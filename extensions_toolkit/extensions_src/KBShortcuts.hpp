
#pragma once

#include "MULOComponent.hpp"

class KBShortcuts : public MULOComponent {
public:
    inline KBShortcuts() { name = "keyboard_shortcuts"; }
    inline ~KBShortcuts() override {}

    inline void init() override { initialized = true; }
    inline void update() override {}
    inline Container* getLayout() { return layout; }
    bool handleEvents();
};

#include "Application.hpp"

bool KBShortcuts::handleEvents() {
    bool forceUpdate = false;

    static bool prevSpace = false;
    static bool prevF11 = false;
    static bool prevCtrl = false;
    static bool prevPlus = false;
    static bool prevMinus = false;

    bool space = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    bool f11 = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F11);
    bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
    bool plus = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Equal);
    bool minus = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Hyphen);

    if (space && !prevSpace) {
        if (app->isPlaying()) {
            app->pause();
            app->setPosition(app->getSavedPosition());
        }
        else
            app->play();

        forceUpdate = true; 
    }

    if (f11 && !prevF11)
        app->requestFullscreenToggle();

    if (ctrl && plus && !prevPlus) {
        app->uiState.uiScale = std::min(1.5f, app->uiState.uiScale + 0.25f);
        app->ui->setScale(app->uiState.uiScale);
    }

    if (ctrl && minus && !prevMinus) {
        app->uiState.uiScale = std::max(0.5f, app->uiState.uiScale - 0.25f);
        app->ui->setScale(app->uiState.uiScale);
    }

    prevSpace = space;
    prevF11 = f11;
    prevCtrl = ctrl;
    prevPlus = plus;
    prevMinus = minus;

    return forceUpdate;
}

// Plugin interface for KBShortcuts
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(KBShortcuts)