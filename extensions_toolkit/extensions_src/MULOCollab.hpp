#pragma once

#include "MULOComponent.hpp"

class MULOCollab : public MULOComponent {
public:
    MULOCollab(){ name = "mulocollab"; }
    ~MULOCollab() override {}

    void init() override;
    void update() override;
    bool handleEvents() override;

    Container* buildLayout();

private:
    sf::RenderWindow window;
    sf::VideoMode resolution;
    sf::View windowView;
    std::unique_ptr<UILO> ui;
    bool pendingClose = false;
    bool pendingUIRebuild = false;

    TextBox* nicknameTextBox = nullptr;
    TextBox* roomNameTextBox = nullptr;
    Text* roomStatusText = nullptr;
    Text* participantsListText = nullptr;
    
    std::string currentRoomName = "";
    std::string lastRoomName = "";
    std::string lastEngineState = "";
    std::string lastStateHash = "";
    std::chrono::steady_clock::time_point lastStateCheck;
    std::vector<std::string> participantsList;
    
    // State change batching
    std::string pendingStateUpdate = "";
    std::chrono::steady_clock::time_point lastChangeTime;
    std::chrono::steady_clock::time_point joinTime;
    bool hasPendingUpdate = false;
    bool wasDragging = false;
    bool justJoinedRoom = false;
    static constexpr int UPDATE_DEBOUNCE_MS = 1000;

    void showWindow();
    void hideWindow();
    bool createRoom();
    bool joinRoom();
};

#include "Application.hpp"

void MULOCollab::init() {
    if (!app) return;

    app->writeConfig<bool>("collabShowWindow", false);
    app->writeConfig<std::string>("collab_nickname", "");
    app->writeConfig<std::string>("collab_room", "");

    resolution.size.x = app->getWindow().getSize().x / 3;
    resolution.size.y = app->getWindow().getSize().y / 1.2;
    windowView.setSize(static_cast<sf::Vector2f>(resolution.size));
    
    lastStateCheck = std::chrono::steady_clock::now();
    lastChangeTime = std::chrono::steady_clock::now();
    hasPendingUpdate = false;
    wasDragging = false;
    justJoinedRoom = false;
    
    initialized = true;
}

void MULOCollab::update() {
    if (nicknameTextBox)
        if (app->readConfig<std::string>("collab_nickname", "") != nicknameTextBox->getText() && !nicknameTextBox->isActive())
            app->writeConfig<std::string>("collab_nickname", nicknameTextBox->getText());

    if (roomNameTextBox)
        if (app->readConfig<std::string>("collab_room", "") != roomNameTextBox->getText() && !roomNameTextBox->isActive())
            app->writeConfig<std::string>("collab_room", roomNameTextBox->getText());

    std::string newRoomName = app->readConfig<std::string>("collab_room", "");
    
    if (newRoomName != currentRoomName) {
        currentRoomName = newRoomName;
        if (!currentRoomName.empty()) {
            justJoinedRoom = true;
            joinTime = std::chrono::steady_clock::now();
            hasPendingUpdate = false;
            app->checkRoomEngineState(currentRoomName);
            lastStateCheck = std::chrono::steady_clock::now();
        }
    }
    
    if (!currentRoomName.empty()) {
        auto now = std::chrono::steady_clock::now();
        bool isDragging = app->ui->isMouseDragging();
        std::string currentEngineState = app->getEngineStateString();
        
        bool stateChanged = (currentEngineState != lastEngineState);
        
        if (stateChanged) {
            pendingStateUpdate = currentEngineState;
            lastChangeTime = now;
            hasPendingUpdate = true;
            lastEngineState = currentEngineState;
        }
        
        bool shouldSendUpdate = false;
        
        if (hasPendingUpdate) {
            if (isDragging) {
                wasDragging = true;
            } else {
                if (wasDragging) {
                    shouldSendUpdate = true;
                    wasDragging = false;
                } else {
                    auto timeSinceChange = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastChangeTime).count();
                    if (timeSinceChange >= UPDATE_DEBOUNCE_MS) {
                        shouldSendUpdate = true;
                    }
                }
            }
        }
        
        if (shouldSendUpdate && hasPendingUpdate && !justJoinedRoom) {
            app->updateRoomEngineState(currentRoomName, pendingStateUpdate);
            hasPendingUpdate = false;
            pendingStateUpdate = "";
        }
        
        if (justJoinedRoom) {
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - joinTime).count() > 2000) {
                justJoinedRoom = false;
            }
        }
        
        auto timeSinceLastCheck = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStateCheck).count();
        if (timeSinceLastCheck > 2000) {
            app->checkRoomEngineState(currentRoomName);
            lastStateCheck = now;
        }
    }

    if (window.isOpen() && ui) {
        if (roomStatusText && currentRoomName != lastRoomName) {
            std::string statusText = currentRoomName.empty() ? "Room: None" : "Room: " + currentRoomName;
            roomStatusText->setString(statusText);
            lastRoomName = currentRoomName;
        }
        
        if (participantsListText) {
            std::string participantsText;
            if (currentRoomName.empty()) {
                participantsText = "No participants";
            } else {
                std::string nickname = app->readConfig<std::string>("collab_nickname", "Anonymous");
                participantsText = nickname;
            }
            participantsListText->setString(participantsText);
        }
        
        ui->forceUpdate(windowView);

        if (ui->windowShouldUpdate()) {
            window.clear(sf::Color(30, 30, 30));
            ui->render();
            window.display();
        }
    }
}

bool MULOCollab::handleEvents() {
    bool shouldForceUpdate = false;
    static bool prevShow = false;
    bool showCollab = app->readConfig<bool>("collabShowWindow", false);

    if (showCollab && !prevShow)
        showWindow();

    if (!showCollab && prevShow)
        hideWindow();

    prevShow = showCollab;
    shouldForceUpdate |= prevShow;

    return shouldForceUpdate;
}

Container* MULOCollab::buildLayout() {
    std::string savedNickname = app->readConfig<std::string>("collab_nickname", "");
    std::string savedRoom = app->readConfig<std::string>("collab_room", "");
    
    nicknameTextBox = textBox(
        Modifier().setfixedHeight(48).setColor(sf::Color::White),
        TBStyle::Pill,
        app->resources.dejavuSansFont,
        "Enter Nickname",
        app->resources.activeTheme->foreground_color,
        app->resources.activeTheme->button_color,
        "nickname_textbox"
    );
    
    if (!savedNickname.empty()) {
        nicknameTextBox->setText(savedNickname);
    }

    roomNameTextBox = textBox(
        Modifier().setfixedHeight(48).setColor(sf::Color::White),
        TBStyle::Pill,
        app->resources.dejavuSansFont,
        "Enter Room Name",
        app->resources.activeTheme->foreground_color,
        app->resources.activeTheme->button_color,
        "room_name_textbox"
    );
    
    if (!savedRoom.empty()) {
        roomNameTextBox->setText(savedRoom);
    }

    auto layout = column(
        Modifier().setColor(app->resources.activeTheme->foreground_color),
    contains{
        row(
            Modifier().align(Align::TOP | Align::CENTER_X).setfixedHeight(96),
        contains{
            spacer(Modifier().setfixedWidth(32).align(Align::CENTER_Y | Align::LEFT)),
            text(
                Modifier().align(Align::CENTER_Y | Align::CENTER_X).setfixedHeight(48).setColor(app->resources.activeTheme->primary_text_color),
                "MULO Collab",
                app->resources.dejavuSansFont,
                "mulo_collab_header_text"
            ),
        }),

        spacer(Modifier().setfixedHeight(16)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::TOP),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Nickname",
                app->resources.dejavuSansFont,
                "nickname_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::TOP),
        contains{
            nicknameTextBox
        }),

        spacer(Modifier().setfixedHeight(16)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::TOP),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Room",
                app->resources.dejavuSansFont,
                "room_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::TOP),
        contains{
            roomNameTextBox
        }),

        spacer(Modifier().setfixedHeight(16)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::TOP),
        contains{
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::LEFT)
                    .onLClick([&](){ 
                        if (roomNameTextBox && nicknameTextBox) {
                            std::string roomName = roomNameTextBox->getText();
                            std::string nickname = nicknameTextBox->getText();
                            if (!roomName.empty() && !nickname.empty()) {
                                app->createRoom(roomName);
                                app->joinRoom(roomName);
                                app->writeConfig<std::string>("collab_room", roomName);
                            }
                        }
                    }),
                ButtonStyle::Pill, 
                "Create", 
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
                        if (roomNameTextBox && nicknameTextBox) {
                            std::string roomName = roomNameTextBox->getText();
                            std::string nickname = nicknameTextBox->getText();
                            if (!roomName.empty() && !nickname.empty()) {
                                app->joinRoom(roomName);
                                app->writeConfig<std::string>("collab_room", roomName);
                            }
                        }
                    }),
                ButtonStyle::Pill, 
                "Join", 
                app->resources.dejavuSansFont, 
                app->resources.activeTheme->secondary_text_color
            ),
        }),

        spacer(Modifier().setfixedHeight(16)),

        row(
            Modifier().setfixedHeight(32).setWidth(0.75f).align(Align::CENTER_X | Align::TOP),
        contains{
            roomStatusText = text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Room: None",
                app->resources.dejavuSansFont,
                "room_status_text"
            ),
        }),

        spacer(Modifier().setfixedHeight(8)),

        row(
            Modifier().setfixedHeight(32).setWidth(0.75f).align(Align::CENTER_X | Align::TOP),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Participants:",
                app->resources.dejavuSansFont,
                "participants_label"
            ),
        }),

        row(
            Modifier().setfixedHeight(120).setWidth(0.75f).align(Align::CENTER_X | Align::TOP),
        contains{
            scrollableColumn(
                Modifier().setColor(app->resources.activeTheme->foreground_color),
            contains{
                participantsListText = text(
                    Modifier().setfixedHeight(24).setColor(app->resources.activeTheme->secondary_text_color),
                    "No participants",
                    app->resources.dejavuSansFont,
                    "participants_list"
                )
            })
        }),

        spacer(Modifier().setfixedHeight(16)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::BOTTOM),
        contains{
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::LEFT)
                    .onLClick([&](){ 
                        std::string currentRoom = app->readConfig<std::string>("collab_room", "");
                        if (!currentRoom.empty()) {
                            app->leaveRoom(currentRoom);
                        }
                        app->writeConfig<std::string>("collab_room", "");
                        if (roomNameTextBox) {
                            roomNameTextBox->setText("");
                        }
                    }),
                ButtonStyle::Pill, 
                "Leave", 
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
                    .onLClick([&](){ app->writeConfig<bool>("collabShowWindow", false); }),
                ButtonStyle::Pill, 
                "Close", 
                app->resources.dejavuSansFont, 
                app->resources.activeTheme->secondary_text_color
            ),
        }),

        spacer(Modifier().setfixedHeight(16)),
    });

    return layout;
}

void MULOCollab::showWindow() {
    if (window.isOpen()) return;

    auto mainPos = app->getWindow().getPosition();
    auto mainSize = app->getWindow().getSize();
    int centerX = mainPos.x + (int(mainSize.x) - int(resolution.size.x)) / 2;
    int centerY = mainPos.y + (int(mainSize.y) - int(resolution.size.y)) / 2;

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    window.create(resolution, "MULO Collab", sf::Style::None, sf::State::Windowed, settings);
    window.setPosition(sf::Vector2i(centerX, centerY));
    window.requestFocus();

    app->ui->setInputBlocked(true);

    ui = std::make_unique<UILO>(window, windowView);
    ui->addPage(page({buildLayout()}), "mulocollab");
    ui->forceUpdate();
}

void MULOCollab::hideWindow() {
    if (!window.isOpen()) return;

    ui.reset();
    window.close();
    cleanupMarkedElements();
    app->ui->setInputBlocked(false);
}

bool MULOCollab::createRoom() {
    return false;
}

bool MULOCollab::joinRoom() {
    return false;
}

GET_INTERFACE
DECLARE_PLUGIN(MULOCollab)