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
        Row* sliderRow = nullptr;
        Slider* scrubber_slider = nullptr;
};

void ScrubberComp::init() {
    scrubber_slider = slider(
        Modifier().setWidth(1.f).setfixedHeight(32).align(Align::CENTER_X | Align::TOP),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Horizontal, "scrubber_slider"
    );

    sliderRow = row(
        Modifier()
            .align(Align::LEFT | Align::TOP),
    contains{
        scrubber_slider,    
    });

    layout = sliderRow;
    parentContainer = app->baseContainer;

    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

bool ScrubberComp::handleEvents() {
    return false;
}

void ScrubberComp::update() {

}

float ScrubberComp::grabValue() {
    return scrubber_slider->getValue();
}

GET_INTERFACE
DECLARE_PLUGIN(ScrubberComp);