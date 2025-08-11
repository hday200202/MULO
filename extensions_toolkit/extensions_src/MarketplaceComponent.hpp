#pragma once

#include "MULOComponent.hpp"
#include "../../src/DebugConfig.hpp"

#include <firebase/app.h>
#include <firebase/firestore.h>

struct ExtensionData {
    std::string id = "";
    std::string author = "Unknown";
    std::string description = "No description provided.";
    std::string downloadURL = "";
    std::string name = "Unnamed Extension";
    std::string version = "0.1.0";
};

class MarketplaceComponent : public MULOComponent {
public:
    MarketplaceComponent();
    ~MarketplaceComponent() override;

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

    std::unique_ptr<firebase::App> firebaseApp;
    firebase::firestore::Firestore* firestore = nullptr;
    firebase::Future<firebase::firestore::QuerySnapshot> extFuture;

    enum class State { Idle, Loading, Success, Error };
    State currentState = State::Idle;
    bool shouldRebuildUI = false;

    std::vector<ExtensionData> extensionList;
    uilo::ScrollableColumn* extensionListContainer = nullptr;

    void initFirebase();
    void fetchExtensions();
    uilo::Container* buildInitialLayout();
    void rebuildExtensionList();
};

#include "Application.hpp"

MarketplaceComponent::MarketplaceComponent() {
    name = "marketplace";
}

MarketplaceComponent::~MarketplaceComponent() {
    if (firestore) {
        delete firestore;
        firestore = nullptr;
    }
}

void MarketplaceComponent::init() {
    resolution.size.x = app->getWindow().getSize().x / 3;
    resolution.size.y = app->getWindow().getSize().y / 1.5;
    windowView.setSize(static_cast<sf::Vector2f>(resolution.size));
    initialized = true;
}

void MarketplaceComponent::initFirebase() {
    firebase::AppOptions options;
    options.set_api_key("AIzaSyCz8-U53Iga6AbMXvB7XMjOSSkqVLGYpOA");
    options.set_app_id("1:1068093358007:web:bdc95a20f8e60375bf7232");
    options.set_project_id("mulo-marketplace");
    options.set_storage_bucket("mulo-marketplace.appspot.com");

    firebaseApp = std::unique_ptr<firebase::App>(firebase::App::Create(options));
    firestore = firebase::firestore::Firestore::GetInstance(firebaseApp.get());
}

void MarketplaceComponent::show() {
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

    if (!firebaseApp) {
        initFirebase();
    }
    
    ui = std::make_unique<uilo::UILO>(window, windowView);
    ui->addPage(page({buildInitialLayout()}), "marketplace");
    
    fetchExtensions();

    extensionListContainer->setOffset(0.f);
}

void MarketplaceComponent::hide() {
    if (!window.isOpen()) return;
    ui.reset();
    window.close();
    uilo::cleanupMarkedElements();
    app->ui->setInputBlocked(false);
}

void MarketplaceComponent::fetchExtensions() {
    if (currentState == State::Loading || !firestore) return;

    currentState = State::Loading;
    extensionList.clear();
    
    shouldRebuildUI = true;
    extFuture = firestore->Collection("extensions").Get();
}

void MarketplaceComponent::update() {
    if (app->uiState.marketplaceShown && !window.isOpen()) {
        show();
    } 
    else if ((!app->uiState.marketplaceShown && window.isOpen()) || pendingClose) {
        hide();
        pendingClose = false;
        app->uiState.marketplaceShown = false;
    }

    if (!window.isOpen()) return;
    
    if (currentState == State::Loading && extFuture.status() == firebase::kFutureStatusComplete) {
        if (extFuture.error() == firebase::firestore::kErrorNone) {
            const auto& snapshot = *extFuture.result();
            for (const auto& doc : snapshot.documents()) {
                ExtensionData data;
                data.id = doc.id();
                if (doc.Get("name").is_string()) data.name = doc.Get("name").string_value();
                if (doc.Get("author").is_string()) data.author = doc.Get("author").string_value();
                if (doc.Get("version").is_string()) data.version = doc.Get("version").string_value();
                if (doc.Get("downloadURL").is_string()) data.downloadURL = doc.Get("downloadURL").string_value();
                extensionList.push_back(data);
            }
            currentState = State::Success;
        } else {
            currentState = State::Error;
        }
        shouldRebuildUI = true;
    }

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

uilo::Container* MarketplaceComponent::buildInitialLayout() {
    using namespace uilo;
    
    extensionListContainer = scrollableColumn(Modifier(), contains{});

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

void MarketplaceComponent::rebuildExtensionList() {
    using namespace uilo;
    if (!extensionListContainer) return;

    extensionListContainer->clear();

    switch(currentState) {
        case State::Loading:
            extensionListContainer->addElement(text(Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedHeight(20), "Loading...", app->resources.dejavuSansFont));
            break;

        case State::Error:
            extensionListContainer->addElement(text(Modifier().align(Align::CENTER_X | Align::CENTER_Y).setColor(sf::Color::Red).setfixedHeight(20), "Failed to load extensions.", app->resources.dejavuSansFont));
            extensionListContainer->addElement(button(Modifier().align(Align::CENTER_X | Align::CENTER_Y).onLClick([&](){ fetchExtensions(); }), ButtonStyle::Pill, "Retry", app->resources.dejavuSansFont));
            break;

        case State::Success:
            if (extensionList.empty()) {
                extensionListContainer->addElement(text(Modifier().align(Align::CENTER_X | Align::CENTER_Y).setfixedHeight(20), "No extensions found.", app->resources.dejavuSansFont));
            } else {
                for (const auto& ext : extensionList) {
                    auto extRow = row(Modifier().setfixedHeight(80), contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                        column(Modifier().align(Align::LEFT | Align::CENTER_Y), contains{
                            text(Modifier().setColor(app->resources.activeTheme->primary_text_color).setfixedHeight(24).align(Align::CENTER_Y), ext.name, app->resources.dejavuSansFont),
                            text(Modifier().setColor(app->resources.activeTheme->secondary_text_color).setfixedHeight(16).align(Align::CENTER_Y), "by " + ext.author + " | v" + ext.version, app->resources.dejavuSansFont)
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

        case State::Idle:
        default:
            break;
    }
    
    extensionListContainer->setOffset(0.f);
    if (ui) ui->forceUpdate();
}

GET_INTERFACE
DECLARE_PLUGIN(MarketplaceComponent)
