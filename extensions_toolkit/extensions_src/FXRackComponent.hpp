#pragma once

#include "MULOComponent.hpp"

class Effect;

class FXRack : public MULOComponent {
public:
    FXRack();
    ~FXRack();

    inline void init() override;
    inline void update() override;
    inline bool handleEvents() override;
    inline Container* getLayout() override { return layout; }
    
private:
    ScrollableRow* fxRackRow = nullptr;
    std::string selectedTrackName = "";
    std::unordered_map<std::string, Button*> enableButtons;
    std::unordered_map<std::string, bool> lastEffectStates;
    int selectedTrackEffectsSize = 0;
    bool pendingPluginOpen = false;
    bool needsUIRebuild = false;

    inline Row* effectRow(const std::string& name, const int index);
    inline void rebuildUI();
};

#include "Application.hpp"
#include "Effect.hpp"

FXRack::FXRack() { name = "fxrack"; }
FXRack::~FXRack() {}

inline void FXRack::init() {
    if (app->baseContainer)
        parentContainer = app->baseContainer;
    else
        return;

    layout = column(
        Modifier().setColor(app->resources.activeTheme->foreground_color).setfixedHeight(128).align(Align::BOTTOM),
        contains{

        }, "base_fxRack_column"
    );

    fxRackRow = scrollableRow(Modifier(), contains{}, "fxRack_scrollable_row");
    fxRackRow->setScrollSpeed(20);
    
    layout->addElement(fxRackRow);

    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

inline void FXRack::update() {
    if (!isVisible()) return;
}

inline bool FXRack::handleEvents() {
    bool forceUpdate = false;
    
    if (app->getSelectedTrackPtr()->getName() != selectedTrackName 
    || app->getSelectedTrackPtr()->getEffectCount() != selectedTrackEffectsSize) {
        selectedTrackName = app->getSelectedTrackPtr()->getName();
        selectedTrackEffectsSize = app->getSelectedTrackPtr()->getEffectCount();
        rebuildUI();
        forceUpdate = true;
    }

    if (needsUIRebuild) {
        rebuildUI();
        needsUIRebuild = false;
        forceUpdate = true;
    }

    return forceUpdate;
}

inline Row* FXRack::effectRow(const std::string& effectName, const int index) {
    sf::Color initialColor = app->resources.activeTheme->middle_color;
    if (index >= 0 && index < app->getSelectedTrackPtr()->getEffectCount()) {
        Effect* effect = app->getSelectedTrackPtr()->getEffect(index);
        if (effect && effect->enabled()) {
            initialColor = app->resources.activeTheme->clip_color;
        }
    }
    
    enableButtons[effectName + effectName + "_" + std::to_string(index)] = button(
        Modifier()
            .setfixedHeight(24)
            .setfixedWidth(24)
            .setColor(initialColor)
            .onLClick([this, index](){
                app->getSelectedTrackPtr()->getEffect(index)->enabled() ?
                app->getSelectedTrackPtr()->getEffect(index)->disable():
                app->getSelectedTrackPtr()->getEffect(index)->enable();
                
                needsUIRebuild = true;
            })
        .align(Align::CENTER_X | Align::CENTER_Y),
        ButtonStyle::Pill,
        "",
        "",
        sf::Color::Transparent,
        effectName + "_" + std::to_string(index)
    );

    auto deleteButton = button(
        Modifier()
            .setfixedHeight(24)
            .setfixedWidth(24)
            .setColor(app->resources.activeTheme->mute_color)
            .align(Align::CENTER_X | Align::CENTER_Y)
            .onLClick([this, index](){
                app->getSelectedTrackPtr()->removeEffect(index);
            }),
        ButtonStyle::Pill,
        "",
        "",
        sf::Color::Transparent,
        effectName + "_" + std::to_string(index) + "_delete"
    );

    return row(
        Modifier()
            .setColor(app->resources.activeTheme->track_color)
            .setfixedWidth(320)
            .setfixedHeight(96)
            .align(Align::CENTER_Y)
            .onLClick([this, index](){
                app->getSelectedTrackPtr()->getEffect(index)->openWindow();
            }),
        contains{
            spacer(Modifier().setfixedWidth(16)),
            column(Modifier().setfixedWidth(24), contains{
                enableButtons[effectName + effectName + "_" + std::to_string(index)],
                spacer(Modifier().setfixedHeight(16).align(Align::CENTER_Y)),
                deleteButton
            }),
            spacer(Modifier().setfixedWidth(24)),
            text(
                Modifier()
                    .setfixedHeight(32)
                    .align(Align::LEFT | Align::CENTER_Y)
                    .setColor(app->resources.activeTheme->primary_text_color),
                effectName,
                app->resources.dejavuSansFont,
                effectName + "_" + std::to_string(index) + "_text"
            )
        }
    );
}

inline void FXRack::rebuildUI() {
    fxRackRow->clear();
    enableButtons.clear();
    lastEffectStates.clear();
    for (auto& e : app->getSelectedTrackPtr()->getEffects())
        fxRackRow->addElements({spacer(Modifier().setfixedWidth(8)), effectRow(e->getName(), e->getIndex())});
}

GET_INTERFACE
DECLARE_PLUGIN(FXRack)