
#pragma once

#include "MULOComponent.hpp"

class UserLogin : public MULOComponent {
public:
    inline UserLogin() { 
        name = "user_login";
        ui = nullptr;
    }

    inline ~UserLogin() override {}

    inline void init() override;
    inline void update() override;
    inline Container* getLayout() { return layout; }
    inline bool handleEvents();

private:
    sf::RenderWindow window;
    sf::VideoMode resolution;
    sf::View windowView;
    std::unique_ptr<UILO> ui;

    Container* buildLayout();
    void showWindow();
    void hideWindow();
};

#include "Application.hpp"

inline void UserLogin::init() {
    if (!app) return;

    app->writeConfig<bool>("show_user_login", false);

    resolution.size.x = app->getWindow().getSize().x / 3;
    resolution.size.y = app->getWindow().getSize().y / 3;
    windowView.setSize(static_cast<sf::Vector2f>(resolution.size));
    initialized = true;
}

inline void UserLogin::update() {
    // testing
    if (window.isOpen() && ui) {
        ui->forceUpdate(windowView);

        if (ui->windowShouldUpdate()) {
            window.clear(sf::Color(30, 30, 30));
            ui->render();
            window.display();
        }
    }
}

inline bool UserLogin::handleEvents() {
    bool shouldForceUpdate = false;

    static bool prevF1 = false;
    bool f1 = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F1);

    if (f1 && !prevF1) {
        app->writeConfig<bool>("show_user_login", !app->readConfig<bool>("show_user_login", false));
    }

    static bool prevShow = false;
    bool showUlogin = app->readConfig<bool>("show_user_login", false);

    if (showUlogin && !prevShow)
        showWindow();

    if (!showUlogin && prevShow)
        hideWindow();

    prevShow = showUlogin;
    prevF1 = f1;
    
    shouldForceUpdate |= prevShow;
    shouldForceUpdate |= prevF1;

    return shouldForceUpdate;
}

Container* UserLogin::buildLayout() {
    auto layout = column(
        Modifier().setColor(app->resources.activeTheme->foreground_color),
    contains{
        row(
            Modifier().align(Align::TOP | Align::LEFT).setfixedHeight(96),
        contains{
            spacer(Modifier().setfixedWidth(32).align(Align::CENTER_Y | Align::LEFT)),
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(48).setColor(app->resources.activeTheme->primary_text_color),
                "MULO Login",
                app->resources.dejavuSansFont,
                "mulo_login_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Username",
                app->resources.dejavuSansFont,
                "username_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill,
                app->resources.dejavuSansFont,
                "Enter Username",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "username_textbox"
            )
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Password",
                app->resources.dejavuSansFont,
                "password_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill,
                app->resources.dejavuSansFont,
                "Enter Password",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "password_textbox"
            )
        })
    });

    return layout;
}

void UserLogin::showWindow() {
    if (window.isOpen()) return;

    auto mainPos = app->getWindow().getPosition();
    auto mainSize = app->getWindow().getSize();
    int centerX = mainPos.x + (int(mainSize.x) - int(resolution.size.x)) / 2;
    int centerY = mainPos.y + (int(mainSize.y) - int(resolution.size.y)) / 2;

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    window.create(resolution, "MULO Login", sf::Style::None, sf::State::Windowed, settings);
    window.setPosition(sf::Vector2i(centerX, centerY));
    window.requestFocus();

    app->ui->setInputBlocked(true);

    ui = std::make_unique<UILO>(window, windowView);
    ui->addPage(page({buildLayout()}), "user_login");
    ui->forceUpdate();
}

void UserLogin::hideWindow() {
    if (!window.isOpen()) return;

    ui.reset();
    window.close();
    cleanupMarkedElements();
    app->ui->setInputBlocked(false);
}

// Plugin interface for UserLogin
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(UserLogin)