#pragma once

#include "MULOComponent.hpp"

class SettingsComponent : public MULOComponent {
public:
    SettingsComponent();
    ~SettingsComponent() override;

    void init() override;
    void update() override;
    Container* getLayout() override { return nullptr; }
    bool handleEvents() override { update(); return false; };

    void show() override;
    void hide() override;

private:
    sf::RenderWindow window;
    sf::VideoMode resolution;
    sf::View windowView;
    std::unique_ptr<UILO> ui;
    bool pendingClose = false;
    bool pendingUIRebuild = false;

    // Temporary settings, before application
    std::string tempSampleRate = "44100";
    std::string tempTheme = "Dark";

    Container* buildLayout();
    void applySettings();
};
