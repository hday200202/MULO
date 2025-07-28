#include "SettingsComponent.hpp"
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
    if (uiState->settingsShown && !window.isOpen()) {
        show();
    } 
    else if ((!uiState->settingsShown && window.isOpen()) || pendingClose) {
        hide();
        pendingClose = false; // Reset immediately after hide
        uiState->settingsShown = false; // ensure settingsShown is false
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
            .setColor(resources->activeTheme->alt_button_color)
            .align(Align::RIGHT | Align::CENTER_Y),
        std::to_string(static_cast<int>(engine->getSampleRate())),
        {"44100", "48000", "96000"},
        resources->dejavuSansFont,
        resources->activeTheme->primary_text_color,
        resources->activeTheme->alt_button_color,
        "sample_rate_dropdown"
    );

    auto uiThemeDropdown = dropdown(
        Modifier()
            .setfixedWidth(resolution.size.x / 3)
            .setfixedHeight(40)
            .setColor(resources->activeTheme->alt_button_color)
            .align(Align::RIGHT | Align::CENTER_Y),
        uiState->selectedTheme,
        Themes::AllThemeNames,
        resources->dejavuSansFont,
        resources->activeTheme->primary_text_color,
        resources->activeTheme->alt_button_color,
        "ui_theme_dropdown"
    );

    auto layout = column(
        Modifier(),
    contains{
        // row(Modifier().setfixedHeight(32), contains{
        //     spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),
        //     text(Modifier().setfixedHeight(24).setColor(resources->activeTheme->primary_text_color).align(Align::CENTER_Y | Align::LEFT), "Settings", resources->dejavuSansFont),
        // }),

        scrollableColumn(
            Modifier().setColor(resources->activeTheme->foreground_color),
            contains{
                row(Modifier().setfixedHeight(64), contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                    text(Modifier().setfixedHeight(48).setColor(resources->activeTheme->primary_text_color).align(Align::CENTER_Y), "Audio", resources->dejavuSansFont),
                }),

                row(Modifier().setfixedHeight(64), contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                    text(Modifier().setfixedHeight(32).setColor(resources->activeTheme->primary_text_color).align(Align::CENTER_Y), "Sample Rate", resources->dejavuSansFont),
                    sampleRateDropdown,
                    spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                }),

                spacer(Modifier().setfixedHeight(16)),

                row(Modifier().setfixedHeight(64), contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                    text(Modifier().setfixedHeight(48).setColor(resources->activeTheme->primary_text_color).align(Align::CENTER_Y), "UI", resources->dejavuSansFont),
                }),

                row(Modifier().setfixedHeight(64), contains{
                    spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                    text(Modifier().setfixedHeight(32).setColor(resources->activeTheme->primary_text_color).align(Align::CENTER_Y), "UI Theme", resources->dejavuSansFont),
                    uiThemeDropdown,
                    spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                }),
            }
        ),

        row(Modifier().setfixedHeight(64).setColor(resources->activeTheme->foreground_color), contains{
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(resources->activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::LEFT)
                    .onLClick([&](){ pendingClose = true; }),
                ButtonStyle::Pill, 
                "close", 
                resources->dejavuSansFont, 
                resources->activeTheme->secondary_text_color
            ),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(resources->activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::RIGHT)
                    .onLClick([&](){ applySettings(); pendingClose = true; }), 
                ButtonStyle::Pill, 
                "apply", 
                resources->dejavuSansFont, 
                resources->activeTheme->secondary_text_color
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
        std::cout << "WARNING: applySettings called recursively, ignoring" << std::endl;
        return;
    }
    applying = true;
    
    std::cout << "DEBUG: applySettings() called" << std::endl;
    
    double sampleRate = std::stod(getDropdown("sample_rate_dropdown")->getSelected());

    if (engine->getSampleRate() != sampleRate)
        engine->setSampleRate(sampleRate);

    if (getDropdown("ui_theme_dropdown")->getSelected() != uiState->selectedTheme) {
        std::string newTheme = getDropdown("ui_theme_dropdown")->getSelected();
        std::cout << "DEBUG: Theme change from " << uiState->selectedTheme << " to " << newTheme << std::endl;
        uiState->selectedTheme = newTheme;

        app->requestUIRebuild();
    }

    uiState->settingsShown = false;
    std::cout << "Settings Applied!\n";
    
    applying = false;
}
