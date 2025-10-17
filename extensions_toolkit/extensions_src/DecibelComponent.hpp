#pragma once
#include "MULOComponent.hpp"
#include "Application.hpp"
#include "../../src/DebugConfig.hpp"

class DecibelComponent : public MULOComponent {
public:
    inline DecibelComponent(){ name = "DecReader" }
    ~DecibelComponent() override{}

    void init() override;
    bool handleEvents() override;
    void update() override;

private:
    Row* decReaderRow = nullptr;
};

void DecibelComponent::init() {
    decReaderRow = row(
        Modifier()
            .setWidth(1.f)
            .setfixedHeight(32)
            .align(Align::CENTER_X | Align::CENTER_Y)
            .setColor(app->resources.activeTheme->track_color),
        contains{}, "decReader_row"

    layout = row(
        Modifier()
            .align(Align::LEFT | Align::TOP)
            .setfixedHeight(48.f)
            .setColor(app->resources.activeTheme->track_color),
    contains{
        decReaderRow,
    );
}

bool DecibelComponent::handleEvents() {
    return false;
}

void DecibelComponent::update() {
    if (!decReaderRow) return;
}