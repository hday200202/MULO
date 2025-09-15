#pragma once
#include "MULOComponent.hpp"
#include "Application.hpp"

class ScrubberComp : public MULOComponent {

    public:
        inline ScrubberComp(){ name = "Scrubber";}
        ~ScrubberComp() override{}

        void init() override;
        bool handleEvents() override;
        void update() override;
        float grabValue();

    private:
        Slider* scrubber_slider;
};

void ScrubberComp::init() {
    scrubber_slider = slider(
        Modifier().setfixedWidth(32).setHeight(1.f).align(Align::CENTER_X | Align::BOTTOM),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Horizontal, "scrubber_slider"
    );
}

bool ScrubberComp::handleEvents() {
    return false;
}

void ScrubberComp::update() {

}

float ScrubberComp::grabValue() {
    return scrubber_slider.getValue();
}

GET_INTERFACE
DECLARE_PLUGIN(ScrubberComp);