#pragma once

#include "MULOComponent.hpp"
#include "../../src/DebugConfig.hpp"

class MarketplaceComponent : public MULOComponent {
public:
    enum class LocalFirebaseState { Idle, Loading, Success, Error };

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
    bool shouldRebuildUI = false;

    std::vector<LocalExtensionData> extensionList;
    uilo::ScrollableColumn* extensionListContainer = nullptr;

    void fetchExtensions();
    uilo::Container* buildInitialLayout();
    void rebuildExtensionList();
};

#include "Application.hpp"

inline void MarketplaceComponent::init() {
    resolution.size.x = app->getWindow().getSize().x / 3;
    resolution.size.y = app->getWindow().getSize().y / 1.5;
    windowView.setSize(static_cast<sf::Vector2f>(resolution.size));
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
        rebuildExtensionList();
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

    return column(Modifier().setColor(app->resources.activeTheme->middle_color), contains{
        row(Modifier().setfixedHeight(48).setColor(app->resources.activeTheme->foreground_color), contains{
            text(Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedHeight(24), "MULO Extension Marketplace", app->resources.dejavuSansFont)
        }),
        extensionListContainer,
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
