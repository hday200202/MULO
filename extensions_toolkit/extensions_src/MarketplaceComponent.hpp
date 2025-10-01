#pragma once

#include "MULOComponent.hpp"
#include "../../src/DebugConfig.hpp"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <vector>
#include <functional>

class MarketplaceComponent : public MULOComponent {
public:
    enum class LocalFirebaseState { Idle, Loading, Success, Error };
    enum class UploadState { Idle, Uploading, Success, Error };

    struct LocalExtensionData {
        std::string id = "";
        std::string author = "Unknown";
        std::string description = "No description provided.";
        std::string downloadURL = "";
        std::string name = "Unnamed Extension";
        std::string version = "0.1.0";
        bool verified = false;
    };

    MarketplaceComponent() { name = "marketplace"; }
    ~MarketplaceComponent() override = default;

    void init() override;
    void update() override;
    uilo::Container* getLayout() override { return nullptr; }
    bool handleEvents() override { return false; }

    void show() override;
    void hide() override;

private:
    sf::RenderWindow window;
    sf::VideoMode resolution;
    sf::View windowView;
    std::unique_ptr<uilo::UILO> ui;
    bool pendingClose = false;

    // State management
    LocalFirebaseState currentState = LocalFirebaseState::Idle;
    UploadState uploadState = UploadState::Idle;
    bool shouldRebuildUI = false;
    bool showUploadSection = false;
    bool showLoginDialog = false;
    bool isRegistering = false;

    std::vector<LocalExtensionData> extensionList;
    uilo::ScrollableColumn* extensionListContainer = nullptr;
    
    // Upload section components
    uilo::TextInput* descriptionInput = nullptr;
    std::string selectedBinaryPath = "";
    std::string selectedSourcePath = "";
    std::string uploadDescription = "";
    std::string uploadErrorMessage = "";
    size_t binaryFileSize = 0;
    size_t sourceFileSize = 0;
    int uploadRetryCount = 0;
    bool networkError = false;
    std::string lastErrorDetails = "";
    
    // Login dialog components
    uilo::TextInput* emailInput = nullptr;
    uilo::TextInput* passwordInput = nullptr;
    uilo::TextInput* displayNameInput = nullptr;
    std::string loginEmail = "";
    std::string loginPassword = "";
    std::string loginDisplayName = "";
    std::string loginMessage = "";

    void fetchExtensions();
    void uploadExtension();
    void selectBinaryFile();
    void selectSourceFile();
    bool validateFiles();
    std::string formatFileSize(size_t bytes);
    bool isValidBinaryFile(const std::string& path);
    bool isValidSourceFile(const std::string& path);
    bool performSecurityScan(const std::string& binaryPath, const std::string& sourcePath);
    bool checkFileSignature(const std::string& path);
    bool scanForMaliciousPatterns(const std::string& content);
    std::string calculateFileHash(const std::string& path);
    void handleUploadError(const std::string& error);
    void resetUploadState();
    bool canRetryUpload() const;
    void showErrorDialog(const std::string& title, const std::string& message);
    void showLogin();
    void hideLogin();
    void performLogin();
    void performRegister();
    void toggleLoginMode();
    uilo::Container* buildInitialLayout();
    uilo::Container* buildUploadSection();
    uilo::Container* buildLoginDialog();
    void rebuildExtensionList();
    void toggleUploadSection();
};

#include "Application.hpp"

inline void MarketplaceComponent::init() {
    resolution.width = app->getWindow().getSize().x / 3;
    resolution.height = app->getWindow().getSize().y / 1.5;
    windowView.setSize(static_cast<sf::Vector2f>(sf::Vector2u(resolution.width, resolution.height)));
    initialized = true;
}

inline void MarketplaceComponent::update() {
    if (app->uiState.marketplaceShown && !window.isOpen()) {
        show();
    } 
    else if ((!app->uiState.marketplaceShown && window.isOpen()) || pendingClose) {
        hide();
        pendingClose = false;
        app->uiState.marketplaceShown = false;
    }

    if (!window.isOpen()) return;
    
    if (shouldRebuildUI) {
        if (showLoginDialog || showUploadSection) {
            // Rebuild the entire UI when switching sections
            ui->clearPages();
            ui->addPage(page({buildInitialLayout()}), "marketplace");
        } else {
            rebuildExtensionList();
        }
        shouldRebuildUI = false;
    }

    if (ui) {
        ui->update(windowView);
        if (ui->windowShouldUpdate()) {
            window.clear(app->resources.activeTheme->middle_color);
            ui->render();
            window.display();
        }
    }
}

inline void MarketplaceComponent::show() {
    using namespace uilo;
    if (window.isOpen()) return;

    auto mainPos = app->getWindow().getPosition();
    auto mainSize = app->getWindow().getSize();
    int centerX = mainPos.x + (int(mainSize.x) - int(resolution.size.x)) / 2;
    int centerY = mainPos.y + (int(mainSize.y) - int(resolution.size.y)) / 2;

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    window.create(resolution, "MULO Marketplace", sf::Style::None, sf::State::Windowed, settings);
    window.setPosition(sf::Vector2i(centerX, centerY));
    window.requestFocus();

    app->ui->setInputBlocked(true);
    
    ui = std::make_unique<uilo::UILO>(window, windowView);
    ui->addPage(page({buildInitialLayout()}), "marketplace");
    
    fetchExtensions();

    extensionListContainer->setOffset(0.f);
}

inline void MarketplaceComponent::hide() {
    if (!window.isOpen()) return;
    ui.reset();
    window.close();
    uilo::cleanupMarkedElements();
    app->ui->setInputBlocked(false);
}

inline void MarketplaceComponent::fetchExtensions() {
    currentState = LocalFirebaseState::Loading;
    shouldRebuildUI = true;
    
    // Use Application Firebase interface
    app->fetchExtensions([this](Application::FirebaseState state, const std::vector<Application::ExtensionData>& extensions) {
        // Convert Application types to local types
        switch (state) {
            case Application::FirebaseState::Idle: currentState = LocalFirebaseState::Idle; break;
            case Application::FirebaseState::Loading: currentState = LocalFirebaseState::Loading; break;
            case Application::FirebaseState::Success: 
                currentState = LocalFirebaseState::Success;
                extensionList.clear();
                for (const auto& ext : extensions) {
                    LocalExtensionData local;
                    local.id = ext.id;
                    local.author = ext.author;
                    local.description = ext.description;
                    local.downloadURL = ext.downloadURL;
                    local.name = ext.name;
                    local.version = ext.version;
                    local.verified = ext.verified;
                    extensionList.push_back(local);
                }
                break;
            case Application::FirebaseState::Error: currentState = LocalFirebaseState::Error; break;
        }
        shouldRebuildUI = true;
    });
}

inline uilo::Container* MarketplaceComponent::buildInitialLayout() {
    using namespace uilo;
    
    extensionListContainer = scrollableColumn(Modifier(), contains{});
    extensionListContainer->setScrollSpeed(40.f);

    auto mainContent = column(Modifier().setColor(app->resources.activeTheme->middle_color), contains{
        row(Modifier().setfixedHeight(48).setColor(app->resources.activeTheme->foreground_color), contains{
            text(Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedHeight(24), "MULO Extension Marketplace", app->resources.dejavuSansFont)
        }),
        row(Modifier().setfixedHeight(48).setColor(app->resources.activeTheme->foreground_color), contains{
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            button(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setfixedWidth(120).setColor(app->resources.activeTheme->button_color)
                    .onLClick([&](){ showUploadSection = false; shouldRebuildUI = true; }),
                ButtonStyle::Pill, "Browse", app->resources.dejavuSansFont
            ),
            button(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setfixedWidth(120).setColor(app->resources.activeTheme->accent_color)
                    .onLClick([&](){ showUploadSection = true; shouldRebuildUI = true; }),
                ButtonStyle::Pill, "Upload", app->resources.dejavuSansFont
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
        }),
        showLoginDialog ? buildLoginDialog() : (showUploadSection ? buildUploadSection() : extensionListContainer),
        row(Modifier().setfixedHeight(64).setColor(app->resources.activeTheme->foreground_color), contains{
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            button(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(40).setfixedWidth(96).setColor(app->resources.activeTheme->button_color)
                    .onLClick([&](){ fetchExtensions(); }),
                ButtonStyle::Pill, "refresh", app->resources.dejavuSansFont
            ),
            button(
                Modifier().align(Align::CENTER_Y | Align::RIGHT).setfixedHeight(40).setfixedWidth(96).setColor(app->resources.activeTheme->mute_color)
                    .onLClick([&](){ pendingClose = true; }),
                ButtonStyle::Pill, "close", app->resources.dejavuSansFont
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
        })
    });

    return mainContent;
}

inline uilo::Container* MarketplaceComponent::buildUploadSection() {
    using namespace uilo;
    
    auto uploadContainer = scrollableColumn(Modifier(), contains{});
    uploadContainer->setScrollSpeed(40.f);
    
    // Title
    uploadContainer->addElement(
        text(Modifier().align(Align::CENTER_X).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color), 
             "Upload Extension", app->resources.dejavuSansFont)
    );
    
    // Authentication status
    if (!app->isUserLoggedIn()) {
        uploadContainer->addElement(
            text(Modifier().align(Align::CENTER_X).setfixedHeight(24).setColor(sf::Color::Red), 
                 "Please log in to upload extensions", app->resources.dejavuSansFont)
        );
        
        uploadContainer->addElement(
            button(
                Modifier().align(Align::CENTER_X).setfixedWidth(120).setfixedHeight(40).setColor(app->resources.activeTheme->accent_color)
                    .onLClick([&](){ showLogin(); }),
                ButtonStyle::Pill, "Login", app->resources.dejavuSansFont
            )
        );
        
        return uploadContainer;
    }
    
    // Show logged in user with logout option
    auto userRow = row(Modifier().setfixedHeight(40), contains{
        text(Modifier().align(Align::LEFT | Align::CENTER_Y).setColor(sf::Color::Green), 
             "Logged in as: " + app->getCurrentUserDisplayName(), app->resources.dejavuSansFont),
        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setfixedWidth(80).setfixedHeight(32).setColor(app->resources.activeTheme->mute_color)
                .onLClick([&](){ app->logoutUser(); shouldRebuildUI = true; }),
            ButtonStyle::Pill, "Logout", app->resources.dejavuSansFont
        )
    });
    uploadContainer->addElement(userRow);
    
    // Security notice
    uploadContainer->addElement(
        text(Modifier().align(Align::CENTER_X).setfixedHeight(40).setColor(sf::Color(255, 165, 0)), 
             "⚠️ All uploads are scanned for security. Malicious code will be rejected.", app->resources.dejavuSansFont)
    );
    
    // Description section
    uploadContainer->addElement(
        text(Modifier().align(Align::LEFT).setfixedHeight(24).setColor(app->resources.activeTheme->primary_text_color), 
             "Description:", app->resources.dejavuSansFont)
    );
    
    descriptionInput = textInput(
        Modifier().setfixedHeight(80).setColor(app->resources.activeTheme->foreground_color)
            .onTextChange([&](const std::string& text){ uploadDescription = text; }),
        "Enter extension description...", app->resources.dejavuSansFont
    );
    uploadContainer->addElement(descriptionInput);
    
    // Binary file selection
    uploadContainer->addElement(
        text(Modifier().align(Align::LEFT).setfixedHeight(24).setColor(app->resources.activeTheme->primary_text_color), 
             "Binary File (.dll/.so/.dylib):", app->resources.dejavuSansFont)
    );
    
    auto binaryRow = row(Modifier().setfixedHeight(48), contains{
        column(Modifier().align(Align::LEFT | Align::CENTER_Y), contains{
            text(Modifier().setColor(app->resources.activeTheme->secondary_text_color).setfixedHeight(20), 
                 selectedBinaryPath.empty() ? "No file selected" : std::filesystem::path(selectedBinaryPath).filename().string(), 
                 app->resources.dejavuSansFont),
            text(Modifier().setColor(app->resources.activeTheme->secondary_text_color).setfixedHeight(16), 
                 selectedBinaryPath.empty() ? "" : formatFileSize(binaryFileSize), 
                 app->resources.dejavuSansFont)
        }),
        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setfixedWidth(120).setfixedHeight(32).setColor(app->resources.activeTheme->button_color)
                .onLClick([&](){ selectBinaryFile(); }),
            ButtonStyle::Pill, "Select Binary", app->resources.dejavuSansFont
        )
    });
    uploadContainer->addElement(binaryRow);
    
    // Source file selection
    uploadContainer->addElement(
        text(Modifier().align(Align::LEFT).setfixedHeight(24).setColor(app->resources.activeTheme->primary_text_color), 
             "Source File (.hpp):", app->resources.dejavuSansFont)
    );
    
    auto sourceRow = row(Modifier().setfixedHeight(48), contains{
        column(Modifier().align(Align::LEFT | Align::CENTER_Y), contains{
            text(Modifier().setColor(app->resources.activeTheme->secondary_text_color).setfixedHeight(20), 
                 selectedSourcePath.empty() ? "No file selected" : std::filesystem::path(selectedSourcePath).filename().string(), 
                 app->resources.dejavuSansFont),
            text(Modifier().setColor(app->resources.activeTheme->secondary_text_color).setfixedHeight(16), 
                 selectedSourcePath.empty() ? "" : formatFileSize(sourceFileSize), 
                 app->resources.dejavuSansFont)
        }),
        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setfixedWidth(120).setfixedHeight(32).setColor(app->resources.activeTheme->button_color)
                .onLClick([&](){ selectSourceFile(); }),
            ButtonStyle::Pill, "Select Source", app->resources.dejavuSansFont
        )
    });
    uploadContainer->addElement(sourceRow);
    
    // Upload status
    std::string statusText = "";
    sf::Color statusColor = app->resources.activeTheme->primary_text_color;
    
    switch(uploadState) {
        case UploadState::Uploading:
            statusText = "Uploading...";
            statusColor = sf::Color::Yellow;
            break;
        case UploadState::Success:
            statusText = "Upload successful!";
            statusColor = sf::Color::Green;
            break;
        case UploadState::Error:
            statusText = uploadErrorMessage.empty() ? "Upload failed. Please try again." : uploadErrorMessage;
            statusColor = sf::Color::Red;
            break;
        case UploadState::Idle:
        default:
            break;
    }
    
    // Show validation errors even when not uploading
    if (!uploadErrorMessage.empty() && uploadState != UploadState::Uploading) {
        uploadContainer->addElement(
            text(Modifier().align(Align::CENTER_X).setfixedHeight(24).setColor(sf::Color::Red), 
                 uploadErrorMessage, app->resources.dejavuSansFont)
        );
        
        // Show retry count if there have been retries
        if (uploadRetryCount > 0) {
            uploadContainer->addElement(
                text(Modifier().align(Align::CENTER_X).setfixedHeight(20).setColor(app->resources.activeTheme->secondary_text_color), 
                     "Retry attempt: " + std::to_string(uploadRetryCount) + "/3", app->resources.dejavuSansFont)
            );
        }
        
        // Show retry button if upload can be retried
        if (canRetryUpload() && uploadState == UploadState::Error) {
            auto retryRow = row(Modifier().setfixedHeight(40), contains{
                button(
                    Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedWidth(100).setfixedHeight(32).setColor(app->resources.activeTheme->accent_color)
                        .onLClick([&](){ uploadExtension(); }),
                    ButtonStyle::Pill, "Retry Upload", app->resources.dejavuSansFont
                ),
                button(
                    Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedWidth(80).setfixedHeight(32).setColor(app->resources.activeTheme->mute_color)
                        .onLClick([&](){ resetUploadState(); }),
                    ButtonStyle::Pill, "Reset", app->resources.dejavuSansFont
                )
            });
            uploadContainer->addElement(retryRow);
        }
    }
    
    if (!statusText.empty()) {
        uploadContainer->addElement(
            text(Modifier().align(Align::CENTER_X).setfixedHeight(24).setColor(statusColor), 
                 statusText, app->resources.dejavuSansFont)
        );
    }
    
    // Show upload progress details
    if (uploadState == UploadState::Uploading) {
        uploadContainer->addElement(
            text(Modifier().align(Align::CENTER_X).setfixedHeight(20).setColor(app->resources.activeTheme->secondary_text_color), 
                 "Uploading files to Firebase Storage...", app->resources.dejavuSansFont)
        );
    }
    
    // Upload button
    bool canUpload = app->isUserLoggedIn() && !uploadDescription.empty() && !selectedBinaryPath.empty() && !selectedSourcePath.empty() && uploadState != UploadState::Uploading;
    
    uploadContainer->addElement(
        button(
            Modifier().align(Align::CENTER_X).setfixedWidth(150).setfixedHeight(40)
                .setColor(canUpload ? app->resources.activeTheme->accent_color : app->resources.activeTheme->mute_color)
                .onLClick([&](){ if (canUpload) uploadExtension(); }),
            ButtonStyle::Pill, "Upload Extension", app->resources.dejavuSansFont
        )
    );
    
    return uploadContainer;
}

inline void MarketplaceComponent::selectBinaryFile() {
#ifdef _WIN32
    std::string path = app->selectFile({"*.dll"});
#elif __APPLE__
    std::string path = app->selectFile({"*.dylib"});
#else
    std::string path = app->selectFile({"*.so"});
#endif
    
    if (!path.empty() && isValidBinaryFile(path)) {
        selectedBinaryPath = path;
        
        // Get file size with error handling
        try {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                binaryFileSize = file.tellg();
                file.close();
                
                // Clear any previous errors
                if (uploadErrorMessage.find("binary") != std::string::npos) {
                    uploadErrorMessage = "";
                }
            } else {
                uploadErrorMessage = "Cannot read binary file";
            }
        } catch (const std::exception& e) {
            uploadErrorMessage = "Error reading binary file: " + std::string(e.what());
        }
    } else if (!path.empty()) {
        uploadErrorMessage = "Invalid binary file selected";
    }
    
    shouldRebuildUI = true;
}

inline void MarketplaceComponent::selectSourceFile() {
    std::string path = app->selectFile({"*.hpp"});
    
    if (!path.empty() && isValidSourceFile(path)) {
        selectedSourcePath = path;
        
        // Get file size with error handling
        try {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                sourceFileSize = file.tellg();
                file.close();
                
                // Clear any previous errors
                if (uploadErrorMessage.find("source") != std::string::npos) {
                    uploadErrorMessage = "";
                }
            } else {
                uploadErrorMessage = "Cannot read source file";
            }
        } catch (const std::exception& e) {
            uploadErrorMessage = "Error reading source file: " + std::string(e.what());
        }
    } else if (!path.empty()) {
        uploadErrorMessage = "Invalid source file selected";
    }
    
    shouldRebuildUI = true;
}

inline void MarketplaceComponent::uploadExtension() {
    if (!validateFiles()) {
        uploadState = UploadState::Error;
        shouldRebuildUI = true;
        return;
    }
    
    uploadState = UploadState::Uploading;
    uploadErrorMessage = "";
    shouldRebuildUI = true;
    
    // Call Application's upload method with enhanced error handling
    app->uploadExtension(uploadDescription, selectedBinaryPath, selectedSourcePath, [&](bool success) {
        if (success) {
            uploadState = UploadState::Success;
            uploadErrorMessage = "";
            uploadRetryCount = 0;
            
            // Clear form on success
            uploadDescription = "";
            selectedBinaryPath = "";
            selectedSourcePath = "";
            binaryFileSize = 0;
            sourceFileSize = 0;
            
            if (descriptionInput) {
                descriptionInput->setText("");
            }
        } else {
            handleUploadError("Upload failed. Please check your connection and try again.");
        }
        shouldRebuildUI = true;
    });
}

inline void MarketplaceComponent::showLogin() {
    showLoginDialog = true;
    isRegistering = false;
    loginEmail = "";
    loginPassword = "";
    loginDisplayName = "";
    loginMessage = "";
    shouldRebuildUI = true;
}

inline void MarketplaceComponent::hideLogin() {
    showLoginDialog = false;
    shouldRebuildUI = true;
}

inline void MarketplaceComponent::performLogin() {
    if (loginEmail.empty() || loginPassword.empty()) {
        loginMessage = "Please enter email and password";
        shouldRebuildUI = true;
        return;
    }
    
    loginMessage = "Logging in...";
    shouldRebuildUI = true;
    
    app->loginUser(loginEmail, loginPassword, [&](bool success, const std::string& message) {
        loginMessage = message;
        if (success) {
            hideLogin();
        }
        shouldRebuildUI = true;
    });
}

inline void MarketplaceComponent::performRegister() {
    if (loginEmail.empty() || loginPassword.empty() || loginDisplayName.empty()) {
        loginMessage = "Please fill in all fields";
        shouldRebuildUI = true;
        return;
    }
    
    loginMessage = "Registering...";
    shouldRebuildUI = true;
    
    app->registerUser(loginEmail, loginPassword, loginDisplayName, [&](bool success, const std::string& message) {
        loginMessage = message;
        if (success) {
            hideLogin();
        }
        shouldRebuildUI = true;
    });
}

inline void MarketplaceComponent::toggleLoginMode() {
    isRegistering = !isRegistering;
    loginMessage = "";
    shouldRebuildUI = true;
}

inline uilo::Container* MarketplaceComponent::buildLoginDialog() {
    using namespace uilo;
    
    auto loginContainer = column(Modifier().setColor(app->resources.activeTheme->middle_color), contains{});
    
    // Title
    loginContainer->addElement(
        text(Modifier().align(Align::CENTER_X).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color), 
             isRegistering ? "Register Account" : "Login", app->resources.dejavuSansFont)
    );
    
    // Email input
    loginContainer->addElement(
        text(Modifier().align(Align::LEFT).setfixedHeight(24).setColor(app->resources.activeTheme->primary_text_color), 
             "Email:", app->resources.dejavuSansFont)
    );
    
    emailInput = textInput(
        Modifier().setfixedHeight(40).setColor(app->resources.activeTheme->foreground_color)
            .onTextChange([&](const std::string& text){ loginEmail = text; }),
        "Enter email address...", app->resources.dejavuSansFont
    );
    loginContainer->addElement(emailInput);
    
    // Password input
    loginContainer->addElement(
        text(Modifier().align(Align::LEFT).setfixedHeight(24).setColor(app->resources.activeTheme->primary_text_color), 
             "Password:", app->resources.dejavuSansFont)
    );
    
    passwordInput = textInput(
        Modifier().setfixedHeight(40).setColor(app->resources.activeTheme->foreground_color)
            .onTextChange([&](const std::string& text){ loginPassword = text; }),
        "Enter password...", app->resources.dejavuSansFont
    );
    loginContainer->addElement(passwordInput);
    
    // Display name input (only for registration)
    if (isRegistering) {
        loginContainer->addElement(
            text(Modifier().align(Align::LEFT).setfixedHeight(24).setColor(app->resources.activeTheme->primary_text_color), 
                 "Display Name:", app->resources.dejavuSansFont)
        );
        
        displayNameInput = textInput(
            Modifier().setfixedHeight(40).setColor(app->resources.activeTheme->foreground_color)
                .onTextChange([&](const std::string& text){ loginDisplayName = text; }),
            "Enter display name...", app->resources.dejavuSansFont
        );
        loginContainer->addElement(displayNameInput);
    }
    
    // Status message
    if (!loginMessage.empty()) {
        sf::Color messageColor = app->resources.activeTheme->primary_text_color;
        if (loginMessage.find("successful") != std::string::npos) {
            messageColor = sf::Color::Green;
        } else if (loginMessage.find("failed") != std::string::npos || loginMessage.find("Please") != std::string::npos) {
            messageColor = sf::Color::Red;
        }
        
        loginContainer->addElement(
            text(Modifier().align(Align::CENTER_X).setfixedHeight(24).setColor(messageColor), 
                 loginMessage, app->resources.dejavuSansFont)
        );
    }
    
    // Buttons
    auto buttonRow = row(Modifier().setfixedHeight(50), contains{
        button(
            Modifier().align(Align::LEFT | Align::CENTER_Y).setfixedWidth(100).setfixedHeight(40).setColor(app->resources.activeTheme->accent_color)
                .onLClick([&](){ isRegistering ? performRegister() : performLogin(); }),
            ButtonStyle::Pill, isRegistering ? "Register" : "Login", app->resources.dejavuSansFont
        ),
        button(
            Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedWidth(120).setfixedHeight(40).setColor(app->resources.activeTheme->button_color)
                .onLClick([&](){ toggleLoginMode(); }),
            ButtonStyle::Pill, isRegistering ? "Switch to Login" : "Switch to Register", app->resources.dejavuSansFont
        ),
        button(
            Modifier().align(Align::RIGHT | Align::CENTER_Y).setfixedWidth(80).setfixedHeight(40).setColor(app->resources.activeTheme->mute_color)
                .onLClick([&](){ hideLogin(); }),
            ButtonStyle::Pill, "Cancel", app->resources.dejavuSansFont
        )
    });
    
    loginContainer->addElement(buttonRow);
    
    return loginContainer;
}

inline bool MarketplaceComponent::validateFiles() {
    uploadErrorMessage = "";
    
    if (uploadDescription.empty()) {
        uploadErrorMessage = "Description is required";
        return false;
    }
    
    if (uploadDescription.length() < 10) {
        uploadErrorMessage = "Description must be at least 10 characters";
        return false;
    }
    
    if (selectedBinaryPath.empty()) {
        uploadErrorMessage = "Binary file is required";
        return false;
    }
    
    if (selectedSourcePath.empty()) {
        uploadErrorMessage = "Source file is required";
        return false;
    }
    
    // Check file sizes (max 50MB for binary, 1MB for source)
    if (binaryFileSize > 50 * 1024 * 1024) {
        uploadErrorMessage = "Binary file too large (max 50MB)";
        return false;
    }
    
    if (sourceFileSize > 1024 * 1024) {
        uploadErrorMessage = "Source file too large (max 1MB)";
        return false;
    }
    
    // Perform security scan with exception handling
    try {
        if (!performSecurityScan(selectedBinaryPath, selectedSourcePath)) {
            return false; // Error message set by performSecurityScan
        }
    } catch (const std::exception& e) {
        uploadErrorMessage = "Security scan failed: " + std::string(e.what());
        return false;
    }
    
    return true;
}

inline std::string MarketplaceComponent::formatFileSize(size_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes / (1024 * 1024)) + " MB";
}

inline bool MarketplaceComponent::isValidBinaryFile(const std::string& path) {
    if (path.empty()) return false;
    
    // Check file extension
    std::string ext = path.substr(path.find_last_of('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
#ifdef _WIN32
    if (ext != ".dll") return false;
#elif __APPLE__
    if (ext != ".dylib") return false;
#else
    if (ext != ".so") return false;
#endif
    
    // Check if file exists and is readable
    std::ifstream file(path, std::ios::binary);
    return file.good();
}

inline bool MarketplaceComponent::isValidSourceFile(const std::string& path) {
    if (path.empty()) return false;
    
    // Check file extension
    std::string ext = path.substr(path.find_last_of('.'));
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    if (ext != ".hpp" && ext != ".h") return false;
    
    // Check if file exists and is readable
    std::ifstream file(path);
    if (!file.good()) return false;
    
    // Basic validation - check if it contains some C++ keywords
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content.find("#pragma") != std::string::npos || 
           content.find("#include") != std::string::npos ||
           content.find("class") != std::string::npos;
}

inline bool MarketplaceComponent::performSecurityScan(const std::string& binaryPath, const std::string& sourcePath) {
    uploadErrorMessage = "";
    
    // Check file signatures
    if (!checkFileSignature(binaryPath)) {
        uploadErrorMessage = "Binary file signature validation failed";
        return false;
    }
    
    // Read and scan source code for malicious patterns
    std::ifstream sourceFile(sourcePath);
    if (!sourceFile.is_open()) {
        uploadErrorMessage = "Cannot read source file for security scan";
        return false;
    }
    
    std::string sourceContent((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());
    sourceFile.close();
    
    if (scanForMaliciousPatterns(sourceContent)) {
        uploadErrorMessage = "Source code contains potentially malicious patterns";
        return false;
    }
    
    return true;
}

inline bool MarketplaceComponent::checkFileSignature(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    // Read first few bytes to check file signature
    char header[16];
    file.read(header, sizeof(header));
    file.close();
    
    // Check for common executable signatures
#ifdef _WIN32
    // Check for PE signature (MZ header)
    return header[0] == 'M' && header[1] == 'Z';
#elif __APPLE__
    // Check for Mach-O signature
    uint32_t magic = *reinterpret_cast<uint32_t*>(header);
    return magic == 0xfeedface || magic == 0xfeedfacf || magic == 0xcafebabe;
#else
    // Check for ELF signature
    return header[0] == 0x7f && header[1] == 'E' && header[2] == 'L' && header[3] == 'F';
#endif
}

inline bool MarketplaceComponent::scanForMaliciousPatterns(const std::string& content) {
    // List of potentially dangerous patterns
    std::vector<std::string> dangerousPatterns = {
        "system(",
        "exec(",
        "popen(",
        "ShellExecute",
        "CreateProcess",
        "WinExec",
        "fork(",
        "eval(",
        "unlink(",
        "remove(",
        "rmdir(",
        "chmod(",
        "chown(",
        "setuid(",
        "setgid(",
        "mmap(",
        "VirtualAlloc",
        "HeapAlloc",
        "malloc(",
        "realloc(",
        "free(",
        "delete",
        "new ",
        "fopen(",
        "fwrite(",
        "fprintf(",
        "sprintf(",
        "strcpy(",
        "strcat(",
        "gets(",
        "scanf(",
        "network",
        "socket(",
        "connect(",
        "bind(",
        "listen(",
        "accept(",
        "send(",
        "recv(",
        "curl",
        "http",
        "https",
        "ftp",
        "registry",
        "RegOpenKey",
        "RegSetValue",
        "RegDeleteKey"
    };
    
    // Convert content to lowercase for case-insensitive search
    std::string lowerContent = content;
    std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
    
    // Check for dangerous patterns
    for (const auto& pattern : dangerousPatterns) {
        if (lowerContent.find(pattern) != std::string::npos) {
            return true; // Found malicious pattern
        }
    }
    
    // Check for suspicious includes
    std::vector<std::string> suspiciousIncludes = {
        "#include <windows.h>",
        "#include <unistd.h>",
        "#include <sys/socket.h>",
        "#include <netinet/in.h>",
        "#include <arpa/inet.h>",
        "#include <sys/mman.h>",
        "#include <sys/stat.h>",
        "#include <fcntl.h>"
    };
    
    for (const auto& include : suspiciousIncludes) {
        if (lowerContent.find(include) != std::string::npos) {
            return true; // Found suspicious include
        }
    }
    
    return false; // No malicious patterns found
}

inline std::string MarketplaceComponent::calculateFileHash(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    
    // Simple hash calculation (in production, use SHA-256 or similar)
    std::hash<std::string> hasher;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    return std::to_string(hasher(content));
}

inline void MarketplaceComponent::handleUploadError(const std::string& error) {
    uploadState = UploadState::Error;
    uploadErrorMessage = error;
    lastErrorDetails = error;
    uploadRetryCount++;
    
    // Log error for debugging
    std::cout << "Upload error: " << error << " (Retry count: " << uploadRetryCount << ")" << std::endl;
    
    // Check for network-related errors
    if (error.find("network") != std::string::npos || 
        error.find("connection") != std::string::npos ||
        error.find("timeout") != std::string::npos) {
        networkError = true;
    }
    
    shouldRebuildUI = true;
}

inline void MarketplaceComponent::resetUploadState() {
    uploadState = UploadState::Idle;
    uploadErrorMessage = "";
    uploadRetryCount = 0;
    networkError = false;
    lastErrorDetails = "";
    selectedBinaryPath = "";
    selectedSourcePath = "";
    uploadDescription = "";
    binaryFileSize = 0;
    sourceFileSize = 0;
    
    if (descriptionInput) {
        descriptionInput->setText("");
    }
    
    shouldRebuildUI = true;
}

inline bool MarketplaceComponent::canRetryUpload() const {
    return uploadRetryCount < 3 && !uploadErrorMessage.empty();
}

inline void MarketplaceComponent::showErrorDialog(const std::string& title, const std::string& message) {
    // In a full implementation, this would show a modal dialog
    // For now, we'll just set the error message
    uploadErrorMessage = title + ": " + message;
    shouldRebuildUI = true;
}

inline void MarketplaceComponent::rebuildExtensionList() {
    using namespace uilo;
    if (!extensionListContainer) return;

    extensionListContainer->clear();

    switch(currentState) {
        case LocalFirebaseState::Loading:
            extensionListContainer->addElement(text(Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedHeight(20), "Loading...", app->resources.dejavuSansFont));
            break;

        case LocalFirebaseState::Error:
            extensionListContainer->addElement(text(Modifier().align(Align::CENTER_X | Align::CENTER_Y).setColor(sf::Color::Red).setfixedHeight(20), "Failed to load extensions.", app->resources.dejavuSansFont));
            extensionListContainer->addElement(button(Modifier().align(Align::CENTER_X | Align::CENTER_Y).onLClick([&](){ fetchExtensions(); }), ButtonStyle::Pill, "Retry", app->resources.dejavuSansFont));
            break;

        case LocalFirebaseState::Success:
            if (extensionList.empty()) {
                extensionListContainer->addElement(text(Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedHeight(20), "No extensions found.", app->resources.dejavuSansFont));
            } else {
                for (const auto& ext : extensionList) {
                    std::string infoText = "by " + ext.author + " | v" + ext.version;
                    if (ext.verified) {
                        infoText += " | VERIFIED";
                    } else {
                        infoText += " | UNVERIFIED";
                    }
                    
                    auto extRow = row(Modifier().setfixedHeight(80), contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                        column(Modifier().align(Align::LEFT | Align::CENTER_Y), contains{
                            text(Modifier().setColor(app->resources.activeTheme->primary_text_color).setfixedHeight(24).align(Align::CENTER_Y), ext.name, app->resources.dejavuSansFont),
                            text(Modifier().setColor(app->resources.activeTheme->secondary_text_color).setfixedHeight(16).align(Align::CENTER_Y), infoText, app->resources.dejavuSansFont)
                        }),
                        button(
                            Modifier().align(Align::RIGHT | Align::CENTER_Y).setfixedWidth(120).setfixedHeight(40).setColor(app->resources.activeTheme->button_color)
                                .onLClick([&, url = ext.downloadURL](){
                                    // Implement file download logic
                                }),
                            ButtonStyle::Pill, "download", app->resources.dejavuSansFont
                        ),
                        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                    });
                    extensionListContainer->addElement(extRow);
                }
            }
            break;

        case LocalFirebaseState::Idle:
        default:
            break;
    }
    
    extensionListContainer->setOffset(0.f);
    if (ui) ui->forceUpdate();
}

GET_INTERFACE
DECLARE_PLUGIN(MarketplaceComponent)
