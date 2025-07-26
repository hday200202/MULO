#pragma once

#include "MULOComponent.hpp"

class TimelineComponent : public MULOComponent {
public:
    TimelineComponent();
    ~TimelineComponent() override;

    void update() override;
    void handleEvents() override;
    uilo::Container* getLayout() override;

private:
    int displayedTrackCount = 0;
    float timelineOffset = 0.f;

    uilo::Container* layout = nullptr;
    uilo::Row* masterTrackElement = nullptr;

    uilo::Row* masterTrack();
    uilo::Row* track(const std::string& trackName, Align alignment, float volume = 1.0f, float pan = 0.0f);

    void handleCustomUIElements() override;
    void rebuildUI() override;
    void rebuildUIFromEngine();
};