
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

    bool pendingRegister = false;
    bool pendingLogin = false;
    bool showRegisterPage = false;

    // Authentication status tracking
    bool isProcessingAuth = false;
    bool showMFAPage = false;
    std::string authStatusMessage = "";
    std::string lastLoginError = "";
    std::string lastRegisterError = "";
    std::string pendingMFAEmail = "";

    TextBox* usernameEmailTextBox = nullptr;
    TextBox* passwordTextBox = nullptr;
    TextBox* emailTextBox = nullptr;
    TextBox* regUsernameTextBox = nullptr;
    TextBox* regPasswordTextBox = nullptr;
    TextBox* confirmPasswordTextBox = nullptr;
    TextBox* mfaCodeTextBox = nullptr;
    
    // MFA code input textboxes (6 digits)
    TextBox* mfaCodeTextBox1 = nullptr;
    TextBox* mfaCodeTextBox2 = nullptr;
    TextBox* mfaCodeTextBox3 = nullptr;
    TextBox* mfaCodeTextBox4 = nullptr;
    TextBox* mfaCodeTextBox5 = nullptr;
    TextBox* mfaCodeTextBox6 = nullptr;
    
    std::string mfaPreviousContent[6] = {"", "", "", "", "", ""};

    Container* buildLoginLayout();
    Container* buildRegisterLayout();
    Container* buildMFALayout();
    void showWindow();
    void hideWindow();
    void handleMFAInput();
};

#include "Application.hpp"

inline void UserLogin::init() {
    if (!app) return;

    app->writeConfig<bool>("show_user_login", false);

    resolution.size.x = app->getWindow().getSize().x / 3;
    resolution.size.y = app->getWindow().getSize().y / 2;
    windowView.setSize(static_cast<sf::Vector2f>(resolution.size));
    initialized = true;
}

inline void UserLogin::update() {
    if (pendingRegister) {
        showRegisterPage = true;
        showMFAPage = false;
        pendingRegister = false;

        hideWindow();
        showWindow();

        if (ui) {
            ui->switchToPage("register_page");
        }
    }
    
    if (pendingLogin) {
        showRegisterPage = false;
        showMFAPage = false;
        pendingLogin = false;

        hideWindow();
        showWindow();

        if (ui) {
            ui->switchToPage("login_page");
        }
    }

    if (showMFAPage) {
        hideWindow();
        showWindow();

        if (ui) {
            ui->switchToPage("mfa_page");
        }
    }

    if (window.isOpen() && ui) {
        // Handle MFA input logic for auto-advance
        handleMFAInput();

        if (ui->getScale() != app->ui->getScale())
            ui->setScale(app->ui->getScale());
            
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

Container* UserLogin::buildLoginLayout() {
    auto layout = column(
        Modifier().setColor(app->resources.activeTheme->foreground_color),
    contains{
        spacer(Modifier().setfixedHeight(24.f).align(Align::TOP)),

        row(
            Modifier().align(Align::TOP | Align::LEFT).setfixedHeight(96),
        contains{
            spacer(Modifier().setfixedWidth(32).align(Align::TOP | Align::LEFT)),
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(48).setColor(app->resources.activeTheme->primary_text_color),
                "MULO Login",
                app->resources.dejavuSansFont,
                "mulo_login_text"
            ),
        }),

        spacer(Modifier().setfixedHeight(24.f).align(Align::TOP)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Username / Email",
                app->resources.dejavuSansFont,
                "username_email_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            usernameEmailTextBox = textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill,
                app->resources.dejavuSansFont,
                "Enter Username or Email",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "username_email_textbox"
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
            passwordTextBox = textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill | TBStyle::Password,
                app->resources.dejavuSansFont,
                "Enter Password",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "password_textbox"
            )
        }),

        spacer(Modifier().setfixedHeight(32.f)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::LEFT)
                    .onLClick([&](){ app->writeConfig<bool>("show_user_login", false); }),
                ButtonStyle::Pill,
                "Close",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::CENTER_X)
                    .onLClick([&](){
                        // Perform login with email/username and password
                        if (usernameEmailTextBox && passwordTextBox && !isProcessingAuth) {
                            std::string email = usernameEmailTextBox->getText();
                            std::string password = passwordTextBox->getText();
                            
                            if (!email.empty() && !password.empty()) {
                                isProcessingAuth = true;
                                authStatusMessage = "Logging in...";
                                
                                app->loginUser(email, password, [&](Application::AuthState state, const std::string& message) {
                                    isProcessingAuth = false;
                                    if (state == Application::AuthState::Success) {
                                        authStatusMessage = "Login successful!";
                                        app->writeConfig<bool>("show_user_login", false);
                                    } else if (state == Application::AuthState::RequiresMFA) {
                                        authStatusMessage = "Enter MFA code";
                                        pendingMFAEmail = email;
                                        showMFAPage = true;
                                        if (ui) {
                                            ui->switchToPage("mfa_page");
                                        }
                                    } else {
                                        lastLoginError = message;
                                        authStatusMessage = "Login failed";
                                    }
                                });
                            } else {
                                authStatusMessage = "Please fill all fields";
                            }
                        }
                    }),
                ButtonStyle::Pill,
                "Login",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::RIGHT)
                    .onLClick([&](){ pendingRegister = true; }),
                ButtonStyle::Pill,
                "Register",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
        })
    });

    return layout;
}

Container* UserLogin::buildRegisterLayout() {
    auto layout = column(
        Modifier().setColor(app->resources.activeTheme->foreground_color),
    contains{
        spacer(Modifier().setfixedHeight(24.f).align(Align::TOP)),

        row(
            Modifier().align(Align::TOP | Align::LEFT).setfixedHeight(96),
        contains{
            spacer(Modifier().setfixedWidth(32).align(Align::TOP | Align::LEFT)),
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(48).setColor(app->resources.activeTheme->primary_text_color),
                "MULO Register",
                app->resources.dejavuSansFont,
                "mulo_register_text"
            ),
        }),

        spacer(Modifier().setfixedHeight(24.f).align(Align::TOP)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Email",
                app->resources.dejavuSansFont,
                "email_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            emailTextBox = textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill,
                app->resources.dejavuSansFont,
                "Enter Email",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "email_textbox"
            )
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Username",
                app->resources.dejavuSansFont,
                "reg_username_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            regUsernameTextBox = textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill,
                app->resources.dejavuSansFont,
                "Enter Username",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "reg_username_textbox"
            )
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Password",
                app->resources.dejavuSansFont,
                "reg_password_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            regPasswordTextBox = textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill | TBStyle::Password,
                app->resources.dejavuSansFont,
                "Enter Password",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "reg_password_textbox"
            )
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Confirm Password",
                app->resources.dejavuSansFont,
                "confirm_password_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            confirmPasswordTextBox = textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill | TBStyle::Password,
                app->resources.dejavuSansFont,
                "Confirm Password",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "confirm_password_textbox"
            )
        }),

        spacer(Modifier().setfixedHeight(32.f)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::LEFT)
                    .onLClick([&](){ app->writeConfig<bool>("show_user_login", false); }),
                ButtonStyle::Pill,
                "Close",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::CENTER_X)
                    .onLClick([&](){ pendingLogin = true; }),
                ButtonStyle::Pill,
                "Login",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::RIGHT)
                    .onLClick([&](){
                        // Perform user registration with validation
                        if (emailTextBox && regUsernameTextBox && regPasswordTextBox && confirmPasswordTextBox && !isProcessingAuth) {
                            std::string email = emailTextBox->getText();
                            std::string username = regUsernameTextBox->getText();
                            std::string password = regPasswordTextBox->getText();
                            std::string confirmPassword = confirmPasswordTextBox->getText();
                            
                            // Basic validation
                            if (email.empty() || username.empty() || password.empty() || confirmPassword.empty()) {
                                authStatusMessage = "Please fill all fields";
                                return;
                            }
                            
                            if (password != confirmPassword) {
                                authStatusMessage = "Passwords do not match";
                                return;
                            }
                            
                            if (password.length() < 6) {
                                authStatusMessage = "Password must be at least 6 characters";
                                return;
                            }
                            
                            isProcessingAuth = true;
                            authStatusMessage = "Creating account...";
                            
                            app->registerUser(email, password, [&](Application::AuthState state, const std::string& message) {
                                isProcessingAuth = false;
                                if (state == Application::AuthState::Success) {
                                    authStatusMessage = "Registration successful!";
                                    // Close the registration window after successful registration
                                    app->writeConfig<bool>("show_user_login", false);
                                } else if (state == Application::AuthState::RequiresMFA) {
                                    authStatusMessage = "Enter verification code";
                                    pendingMFAEmail = email;
                                    showMFAPage = true;
                                } else {
                                    lastRegisterError = message;
                                    authStatusMessage = "Registration failed";
                                }
                            });
                        }
                    }),
                ButtonStyle::Pill,
                "Register",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
        })
    });

    return layout;
}

Container* UserLogin::buildMFALayout() {
    auto layout = column(
        Modifier().setColor(app->resources.activeTheme->foreground_color),
    contains{
        spacer(Modifier().setfixedHeight(24.f).align(Align::TOP)),

        row(
            Modifier().align(Align::TOP | Align::LEFT).setfixedHeight(96),
        contains{
            spacer(Modifier().setfixedWidth(32).align(Align::TOP | Align::LEFT)),
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(48).setColor(app->resources.activeTheme->primary_text_color),
                "MULO 2FA",
                app->resources.dejavuSansFont,
                "mfa_header_text"
            ),
        }),

        spacer(Modifier().setfixedHeight(24.f).align(Align::TOP)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::CENTER_X).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Verification Code",
                app->resources.dejavuSansFont,
                "mfa_instruction_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(1.f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            mfaCodeTextBox1 = textBox(
                Modifier().setfixedHeight(64).setfixedWidth(64).setColor(sf::Color::White).align(Align::CENTER_X),
                TBStyle::CenterText,
                app->resources.dejavuSansFont,
                "",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "mfa_code_textbox1"
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::CENTER_X)),
            mfaCodeTextBox2 = textBox(
                Modifier().setfixedHeight(64).setfixedWidth(64).setColor(sf::Color::White).align(Align::CENTER_X),
                TBStyle::CenterText,
                app->resources.dejavuSansFont,
                "",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "mfa_code_textbox2"
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::CENTER_X)),
            mfaCodeTextBox3 = textBox(
                Modifier().setfixedHeight(64).setfixedWidth(64).setColor(sf::Color::White).align(Align::CENTER_X),
                TBStyle::CenterText,
                app->resources.dejavuSansFont,
                "",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "mfa_code_textbox3"
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::CENTER_X)),
            mfaCodeTextBox4 = textBox(
                Modifier().setfixedHeight(64).setfixedWidth(64).setColor(sf::Color::White).align(Align::CENTER_X),
                TBStyle::CenterText,
                app->resources.dejavuSansFont,
                "",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "mfa_code_textbox4"
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::CENTER_X)),
            mfaCodeTextBox5 = textBox(
                Modifier().setfixedHeight(64).setfixedWidth(64).setColor(sf::Color::White).align(Align::CENTER_X),
                TBStyle::CenterText,
                app->resources.dejavuSansFont,
                "",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "mfa_code_textbox5"
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::CENTER_X)),
            mfaCodeTextBox6 = textBox(
                Modifier().setfixedHeight(64).setfixedWidth(64).setColor(sf::Color::White).align(Align::CENTER_X),
                TBStyle::CenterText,
                app->resources.dejavuSansFont,
                "",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "mfa_code_textbox6"
            )
        }),

        spacer(Modifier().setfixedHeight(32.f).align(Align::CENTER_Y)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::LEFT)
                    .onLClick([&](){ app->writeConfig<bool>("show_user_login", false); }),
                ButtonStyle::Pill,
                "Cancel",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::CENTER_X)
                    .onLClick([&](){ pendingLogin = true; }),
                ButtonStyle::Pill,
                "Back",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::RIGHT)
                    .onLClick([&](){
                        if (mfaCodeTextBox1 && mfaCodeTextBox2 && mfaCodeTextBox3 && 
                            mfaCodeTextBox4 && mfaCodeTextBox5 && mfaCodeTextBox6 && !isProcessingAuth) {
                            
                            std::string code = mfaCodeTextBox1->getText() + 
                                             mfaCodeTextBox2->getText() + 
                                             mfaCodeTextBox3->getText() + 
                                             mfaCodeTextBox4->getText() + 
                                             mfaCodeTextBox5->getText() + 
                                             mfaCodeTextBox6->getText();
                            
                            if (code.length() == 6) {
                                isProcessingAuth = true;
                                authStatusMessage = "Verifying...";
                                
                                app->verifyMFA(code, [&](Application::AuthState state, const std::string& message) {
                                    isProcessingAuth = false;
                                    if (state == Application::AuthState::Success) {
                                        authStatusMessage = "MFA verification successful!";
                                        app->writeConfig<bool>("show_user_login", false);
                                    } else {
                                        authStatusMessage = "Invalid verification code";
                                    }
                                });
                            } else {
                                authStatusMessage = "Please enter all 6 digits";
                            }
                        }
                    }),
                ButtonStyle::Pill,
                "Verify",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
        })
    });

    return layout;
}

void UserLogin::showWindow() {
    if (window.isOpen()) return;

    if (showRegisterPage) {
        resolution = sf::VideoMode::getDesktopMode();
        resolution.size.x = app->getWindow().getSize().x / 3;
        resolution.size.y = app->getWindow().getSize().y / 1.3;
    }

    else {
        resolution = sf::VideoMode::getDesktopMode();
        resolution.size.x = app->getWindow().getSize().x / 3;
        resolution.size.y = app->getWindow().getSize().y / 2;
    }

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
    ui->addPage(page({buildLoginLayout()}), "login_page");
    ui->addPage(page({buildRegisterLayout()}), "register_page");
    ui->addPage(page({buildMFALayout()}), "mfa_page");
    ui->switchToPage("login_page");
    ui->forceUpdate();
}

void UserLogin::hideWindow() {
    if (!window.isOpen()) return;

    ui.reset();
    window.close();
    cleanupMarkedElements();
    app->ui->setInputBlocked(false);
}

void UserLogin::handleMFAInput() {
    if (!showMFAPage || !TextBox::s_activeTextBox) return;    
    
    TextBox* mfaBoxes[] = {mfaCodeTextBox1, mfaCodeTextBox2, mfaCodeTextBox3, 
                          mfaCodeTextBox4, mfaCodeTextBox5, mfaCodeTextBox6};
    
    int activeIndex = -1;
    for (int i = 0; i < 6; i++) {
        if (mfaBoxes[i] == TextBox::s_activeTextBox) {
            activeIndex = i;
            break;
        }
    }
    
    if (activeIndex == -1) return;
    
    TextBox* activeBox = mfaBoxes[activeIndex];
    std::string currentText = activeBox->getText();
    
    if (currentText.length() > 1) {
        std::string lastChar = currentText.substr(currentText.length() - 1);
        if (std::isdigit(lastChar[0])) {
            activeBox->setText(lastChar);
            activeBox->setCursorPosition(1);
            currentText = lastChar;
            
            if (activeIndex < 5) {
                TextBox* nextBox = mfaBoxes[activeIndex + 1];
                if (nextBox) {
                    activeBox->setActive(false);
                    nextBox->setActive(true);
                    TextBox::s_activeTextBox = nextBox;
                }
            }
        } else {
            activeBox->setText("");
            activeBox->setCursorPosition(0);
            currentText = "";
        }
    } else if (currentText.length() == 1) {
        if (!std::isdigit(currentText[0])) {
            activeBox->setText("");
            activeBox->setCursorPosition(0);
            currentText = "";
        } else {
            if (mfaPreviousContent[activeIndex].empty() && activeIndex < 5) {
                TextBox* nextBox = mfaBoxes[activeIndex + 1];
                if (nextBox) {
                    activeBox->setActive(false);
                    nextBox->setActive(true);
                    TextBox::s_activeTextBox = nextBox;
                }
            }
        }
    }
    
    mfaPreviousContent[activeIndex] = currentText;
}

// Plugin interface for UserLogin
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(UserLogin)