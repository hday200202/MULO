#include "TimelineComponent.hpp"
#include "Application.hpp"

TimelineComponent::TimelineComponent() {
    layout = scrollableColumn(Modifier(), contains{}, "timeline");
}

TimelineComponent::~TimelineComponent() {}

void TimelineComponent::update() {
    
}

uilo::Container* TimelineComponent::getLayout() {
    return scrollableColumn(
        Modifier(),
    contains{
    }, "timeline" );
}

uilo::Row* TimelineComponent::masterTrack() {
    return row(
        Modifier()
            .setColor(track_row_color)
            .setfixedHeight(96)
            .align(Align::LEFT | Align::BOTTOM),
    contains{
        column(
            Modifier()
                .align(Align::RIGHT)
                .setfixedWidth(150)
                .setColor(master_track_color),
        contains{
            spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

            row(
                Modifier(),
            contains{
                spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

                column(
                    Modifier(),
                contains{
                    text(
                        Modifier().setColor(primary_text_color).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                        "Master",
                        resources->dejavuSansFont
                    ),

                    row(
                        Modifier(),
                    contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                        button(
                            Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(64).setfixedHeight(32).setColor(not_muted_color),
                            ButtonStyle::Rect,
                            "mute",
                            resources->dejavuSansFont,
                            secondary_text_color,
                            "mute_Master"
                        ),
                    }),
                }),

                slider(
                    Modifier().setfixedWidth(16).setHeight(1.f).align(Align::RIGHT | Align::CENTER_Y),
                    slider_knob_color,
                    slider_bar_color,
                    SliderOrientation::Vertical,
                    "Master_volume_slider"
                ),

                spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            }, "Master_Track_Label"),

            spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
        }, "Master_Track_Column")
    }, "Master_Track");
}

uilo::Row* TimelineComponent::track(
    const std::string& trackName, 
    Align alignment, 
    float volume, 
    float pan
) {
    return row(
        Modifier()
            .setColor(track_row_color)
            .setfixedHeight(96)
            .align(alignment),
    contains{
        scrollableRow(
            Modifier().setHeight(1.f).align(Align::LEFT).setColor(sf::Color::Transparent),
        contains {
            // contains nothing, really just to get the offset from scroll
        }, trackName + "_scrollable_row"),

        column(
            Modifier()
                .align(Align::RIGHT)
                .setfixedWidth(150)
                .setColor(track_color),
        contains{
            spacer(Modifier().setfixedHeight(12).align(Align::TOP)),

            row(
                Modifier().align(Align::RIGHT),
            contains{
                spacer(Modifier().setfixedWidth(8).align(Align::LEFT)),

                column(
                    Modifier(),
                contains{
                    text(
                        Modifier().setColor(primary_text_color).setfixedHeight(24).align(Align::LEFT | Align::TOP),
                        trackName,
                        resources->dejavuSansFont
                    ),

                    row(
                        Modifier(),
                    contains{
                        spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),

                        button(
                            Modifier().align(Align::LEFT | Align::BOTTOM).setfixedWidth(64).setfixedHeight(32).setColor(not_muted_color),
                            ButtonStyle::Rect,
                            "mute",
                            resources->dejavuSansFont,
                            secondary_text_color,
                            "mute_" + trackName
                        ),
                    }),
                }),

                slider(
                    Modifier().setfixedWidth(16).setHeight(1.f).align(Align::RIGHT | Align::CENTER_Y),
                    slider_knob_color,
                    slider_bar_color,
                    SliderOrientation::Vertical,
                    trackName + "_volume_slider"
                ),

                spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
            }),

            spacer(Modifier().setfixedHeight(8).align(Align::BOTTOM)),
        })
    }, trackName + "_track_row");
}

void TimelineComponent::handleEvents() {
    app->shouldForceUpdate = false;

    if (getButton("mute_Master") && getButton("mute_Master")->isClicked()) {
        engine->getMasterTrack()->toggleMute();
        getButton("mute_Master")->m_modifier.setColor((engine->getMasterTrack()->isMuted() ? mute_color : not_muted_color));
        std::cout << "Master track mute state toggled to " << ((engine->getMasterTrack()->isMuted()) ? "true" : "false") << std::endl;

        app->shouldForceUpdate = true;
    }

    // Handle master track solo button
    if (getButton("solo_Master") && getButton("solo_Master")->isClicked()) {
        bool wasSolo = engine->getMasterTrack()->isSolo();
        
        if (wasSolo) {
            // Un-solo master track
            engine->getMasterTrack()->setSolo(false);
        } else {
            // Solo master track and un-solo all other tracks
            engine->getMasterTrack()->setSolo(true);
            for (auto& track : engine->getAllTracks()) {
                track->setSolo(false);
            }
        }
        
        // Update button colors
        if (getButton("solo_Master")) {
            getButton("solo_Master")->m_modifier.setColor(
                (engine->getMasterTrack()->isSolo() ? mute_color : button_color)
            );
        }
        for (auto& track : engine->getAllTracks()) {
            if (getButton("solo_" + track->getName())) {
                getButton("solo_" + track->getName())->m_modifier.setColor(
                    (track->isSolo() ? mute_color : button_color)
                );
            }
        }

        std::cout << "Master track solo state toggled to " << ((engine->getMasterTrack()->isSolo()) ? "true" : "false") << std::endl;
        app->shouldForceUpdate = true;
    }

    if (getSlider("Master_volume_slider")->getValue() != decibelsToFloat(engine->getMasterTrack()->getVolume())) {
        float newVolume = floatToDecibels(getSlider("Master_volume_slider")->getValue());
        engine->getMasterTrack()->setVolume(newVolume);
        getSlider("Master_mixer_volume_slider")->setValue(getSlider("Master_volume_slider")->getValue());
        std::cout << "Master track volume changed to: " << newVolume << " db" << std::endl;

        app->shouldForceUpdate = true;
    }

    if (getSlider("Master_mixer_volume_slider")->getValue() != decibelsToFloat(engine->getMasterTrack()->getVolume())) {
        float newVolume = floatToDecibels(getSlider("Master_mixer_volume_slider")->getValue());
        engine->getMasterTrack()->setVolume(newVolume);
        getSlider("Master_volume_slider")->setValue(getSlider("Master_mixer_volume_slider")->getValue());
        std::cout << "Master track volume changed to: " << newVolume << " db" << std::endl;

        app->shouldForceUpdate = true;
    }

    // Handle Master pan sliders (mixer only)
    float masterSliderValue = (engine->getMasterTrack()->getPan() + 1.0f) / 2.0f;
    if (getSlider("Master_mixer_pan_slider")->getValue() != masterSliderValue) {
        float sliderPan = getSlider("Master_mixer_pan_slider")->getValue();
        float newPan = (sliderPan * 2.0f) - 1.0f; // Convert slider (0-1) to engine (-1 to 1)
        engine->getMasterTrack()->setPan(newPan);
        std::cout << "Master track pan changed to: " << newPan << " (slider: " << sliderPan << ")" << std::endl;

        app->shouldForceUpdate = true;
    }

    for (auto& track : engine->getAllTracks()) {
        if (getButton("mute_" + track->getName())->isClicked()) {
            track->toggleMute();
            getButton("mute_" + track->getName())->m_modifier.setColor((track->isMuted() ? mute_color : not_muted_color));
            std::cout << "Track '" << track->getName() << "' mute state toggled to " << ((track->isMuted()) ? "true" : "false") << std::endl;

            app->shouldForceUpdate = true;
        }

        // Handle solo button clicks
        if (getButton("solo_" + track->getName()) && getButton("solo_" + track->getName())->isClicked()) {
            bool wasSolo = track->isSolo();
            
            // If this track was the only one soloed, un-solo it
            if (wasSolo) {
                // Check if this is the only soloed track
                bool isOnlySoloedTrack = true;
                for (auto& otherTrack : engine->getAllTracks()) {
                    if (otherTrack != track && otherTrack->isSolo()) {
                        isOnlySoloedTrack = false;
                        break;
                    }
                }
                
                if (isOnlySoloedTrack) {
                    // Un-solo this track
                    track->setSolo(false);
                } else {
                    // There are other soloed tracks, so just solo this one (un-solo others)
                    for (auto& otherTrack : engine->getAllTracks()) {
                        otherTrack->setSolo(otherTrack == track);
                    }
                }
            } else {
                // Solo this track and un-solo all others
                for (auto& otherTrack : engine->getAllTracks()) {
                    otherTrack->setSolo(otherTrack == track);
                }
            }
            
            // Update button colors for all tracks
            for (auto& updateTrack : engine->getAllTracks()) {
                if (getButton("solo_" + updateTrack->getName())) {
                    getButton("solo_" + updateTrack->getName())->m_modifier.setColor(
                        (updateTrack->isSolo() ? mute_color : button_color)
                    );
                }
            }
            
            std::cout << "Track '" << track->getName() << "' solo state toggled to " << ((track->isSolo()) ? "true" : "false") << std::endl;
            app->shouldForceUpdate = true;
        }

        if (floatToDecibels(getSlider(track->getName() + "_volume_slider")->getValue()) != track->getVolume()) {
            float newVolume = floatToDecibels(getSlider(track->getName() + "_volume_slider")->getValue());
            track->setVolume(newVolume);
            getSlider(track->getName() + "_mixer_volume_slider")->setValue(getSlider(track->getName() + "_volume_slider")->getValue());
            std::cout << "Track '" << track->getName() << "' volume changed to: " << newVolume << " db" << std::endl;

            app->shouldForceUpdate = true;
        }

        if (floatToDecibels(getSlider(track->getName() + "_mixer_volume_slider")->getValue()) != track->getVolume()) {
            float newVolume = floatToDecibels(getSlider(track->getName() + "_mixer_volume_slider")->getValue());
            track->setVolume(newVolume);
            getSlider(track->getName() + "_volume_slider")->setValue(getSlider(track->getName() + "_mixer_volume_slider")->getValue());
            std::cout << "Track '" << track->getName() << "' volume changed to: " << newVolume << " db" << std::endl;

            app->shouldForceUpdate = true;
        }

        // Handle track pan sliders (mixer only)
        float trackSliderValue = (track->getPan() + 1.0f) / 2.0f;
        if (getSlider(track->getName() + "_mixer_pan_slider")->getValue() != trackSliderValue) {
            float sliderPan = getSlider(track->getName() + "_mixer_pan_slider")->getValue();
            float newPan = (sliderPan * 2.0f) - 1.0f; // Convert slider (0-1) to engine (-1 to 1)
            track->setPan(newPan);
            std::cout << "Track '" << track->getName() << "' pan changed to: " << newPan << " (slider: " << sliderPan << ")" << std::endl;

            app->shouldForceUpdate = true;
        }
    }
}

void TimelineComponent::handleCustomUIElements() {
    
}

void TimelineComponent::rebuildUI() {

}