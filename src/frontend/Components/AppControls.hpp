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