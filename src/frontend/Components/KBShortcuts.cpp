#include "KBShortcuts.hpp"
#include "Application.hpp"

bool KBShortcuts::handleEvents() {
    bool forceUpdate = false;

    // Play/Pause
    static bool prevSpace = false;
    static bool prevF11 = false;
    bool space = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
    bool f11 = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F11);

    if (space && !prevSpace) {
        if (engine->isPlaying())
            engine->pause();
        else
            engine->play();

        forceUpdate = true; 
    }

    if (f11 && !prevF11)
        app->requestFullscreenToggle();

    prevSpace = space;
    prevF11 = f11;

    return forceUpdate;
}