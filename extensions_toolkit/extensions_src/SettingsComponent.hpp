
#pragma once

#include "MULOComponent.hpp"
#include "../../src/DebugConfig.hpp"

class SettingsComponent : public MULOComponent {
public:
    SettingsComponent();
    ~SettingsComponent() override;

    void init() override;
    void update() override;
    Container* getLayout() override { return nullptr; }
    bool handleEvents() override { update(); return false; }

    void show() override;
    void hide() override;

private:
    sf::RenderWindow window;
    sf::VideoMode resolution;
    sf::View windowView;
    std::unique_ptr<UILO> ui;
    bool pendingClose = false;
    bool pendingUIRebuild = false;

    std::string tempSampleRate = "44100";
    std::string tempTheme = "Dark";

    Container* buildLayout();
    void applySettings();
};

#include "Application.hpp"

SettingsComponent::SettingsComponent() {
    name = "settings";
    ui = nullptr;
}

SettingsComponent::~SettingsComponent() {}

void SettingsComponent::init() {
    resolution.size.x = app->getWindow().getSize().x / 3;
    resolution.size.y = app->getWindow().getSize().y / 1.5;
    windowView.setSize(static_cast<sf::Vector2f>(resolution.size));
    initialized = true;
}

void SettingsComponent::update() {
    if (app->uiState.settingsShown && !window.isOpen()) {
        show();
    } 
    else if ((!app->uiState.settingsShown && window.isOpen()) || pendingClose) {
        hide();
        pendingClose = false;
        app->uiState.settingsShown = false;
    }

    if (window.isOpen() && ui) {
        ui->forceUpdate(windowView);

        if (ui->windowShouldUpdate()) {
            window.clear(sf::Color(30, 30, 30));
            ui->render();
            window.display();
        }
    }
}

Container* SettingsComponent::buildLayout() {
    auto sampleRateDropdown = dropdown(
        Modifier()
            .setfixedWidth(resolution.size.x / 3)
            .setfixedHeight(40)
            .setColor(app->resources.activeTheme->alt_button_color)
            .align(Align::RIGHT | Align::CENTER_Y),
        std::to_string(static_cast<int>(app->getSampleRate())),
        {"44100", "48000", "96000"},
        app->resources.dejavuSansFont,
        app->resources.activeTheme->primary_text_color,
        app->resources.activeTheme->alt_button_color,
        "sample_rate_dropdown"
    );

    auto uiThemeDropdown = dropdown(
        Modifier()
            .setfixedWidth(resolution.size.x / 3)
            .setfixedHeight(40)
            .setColor(app->resources.activeTheme->alt_button_color)
            .align(Align::RIGHT | Align::CENTER_Y),
        app->uiState.selectedTheme,
        Themes::AllThemeNames,
        app->resources.dejavuSansFont,
        app->resources.activeTheme->primary_text_color,
        app->resources.activeTheme->alt_button_color,
        "ui_theme_dropdown"
    );

    auto layout = column(
        Modifier(),
    contains{
        scrollableColumn(
            Modifier().setColor(app->resources.activeTheme->foreground_color),
            contains{
                row(Modifier().setfixedHeight(64), contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                    text(Modifier().setfixedHeight(48).setColor(app->resources.activeTheme->primary_text_color).align(Align::CENTER_Y), "Audio", app->resources.dejavuSansFont),
                }),

                row(Modifier().setfixedHeight(64), contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                    text(Modifier().setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color).align(Align::CENTER_Y), "Sample Rate", app->resources.dejavuSansFont),
                    sampleRateDropdown,
                    spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                }),

                spacer(Modifier().setfixedHeight(16)),

                row(Modifier().setfixedHeight(64), contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                    text(Modifier().setfixedHeight(48).setColor(app->resources.activeTheme->primary_text_color).align(Align::CENTER_Y), "UI", app->resources.dejavuSansFont),
                }),

                row(Modifier().setfixedHeight(64), contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                    text(Modifier().setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color).align(Align::CENTER_Y), "UI Theme", app->resources.dejavuSansFont),
                    uiThemeDropdown,
                    spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                }),
            }
        ),

        row(Modifier().setfixedHeight(64).setColor(app->resources.activeTheme->foreground_color), contains{
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::LEFT)
                    .onLClick([&](){ pendingClose = true; }),
                ButtonStyle::Pill, 
                "close", 
                app->resources.dejavuSansFont, 
                app->resources.activeTheme->secondary_text_color
            ),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::RIGHT)
                    .onLClick([&](){ applySettings(); pendingClose = true; }), 
                ButtonStyle::Pill, 
                "apply", 
                app->resources.dejavuSansFont, 
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
        }),
    });

    return layout;
}

void SettingsComponent::show() {
    if (window.isOpen()) return;

    auto mainPos = app->getWindow().getPosition();
    auto mainSize = app->getWindow().getSize();
    int centerX = mainPos.x + (int(mainSize.x) - int(resolution.size.x)) / 2;
    int centerY = mainPos.y + (int(mainSize.y) - int(resolution.size.y)) / 2;

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    window.create(resolution, "MULO Settings", sf::Style::None, sf::State::Windowed, settings);
    window.setPosition(sf::Vector2i(centerX, centerY));
    window.requestFocus();

    app->ui->setInputBlocked(true);

    ui = std::make_unique<UILO>(window, windowView);
    ui->addPage(page({buildLayout()}), "settings");
    ui->forceUpdate();
}

void SettingsComponent::hide() {
    if (!window.isOpen()) return;

    ui.reset();
    window.close();
    cleanupMarkedElements();
    app->ui->setInputBlocked(false);
}

void SettingsComponent::applySettings() {
    static bool applying = false;
    if (applying) {
        DEBUG_PRINT("WARNING: applySettings called recursively, ignoring");
        return;
    }
    applying = true;
    
    double sampleRate = std::stod(getDropdown("sample_rate_dropdown")->getSelected());

    if (app->getSampleRate() != sampleRate)
        app->setSampleRate(sampleRate);

    if (getDropdown("ui_theme_dropdown")->getSelected() != app->uiState.selectedTheme) {
        std::string newTheme = getDropdown("ui_theme_dropdown")->getSelected();
        app->uiState.selectedTheme = newTheme;

        app->requestUIRebuild();
    }

    app->uiState.settingsShown = false;
    DEBUG_PRINT("Settings Applied!");
    
    applying = false;
}

// Plugin interface for SettingsComponent
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(SettingsComponent)
