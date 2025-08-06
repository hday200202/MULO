
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
    bool space = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    bool f11 = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F11);

    if (space && !prevSpace) {
        if (app->isPlaying())
            app->pause();
        else
            app->play();

        forceUpdate = true; 
    }

    if (f11 && !prevF11)
        app->requestFullscreenToggle();

    prevSpace = space;
    prevF11 = f11;

    return forceUpdate;
}

// Plugin interface for KBShortcuts
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(KBShortcuts)