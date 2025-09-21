#pragma once
#include "MULOComponent.hpp"
#include "Application.hpp"
#include "../../src/DebugConfig.hpp"

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
    float lastValue = 0.f;
};

void ScrubberComp::init() {
    MULOComponent* timeline = app->getComponent("timeline");

    if (!timeline) return;

    scrubber_slider = slider(
        Modifier().setWidth(1.f).setfixedHeight(32).align(Align::CENTER_X | Align::CENTER_Y),
        app->resources.activeTheme->slider_knob_color,
        app->resources.activeTheme->slider_bar_color,
        SliderOrientation::Horizontal, "scrubber_slider"
    );

    scrubber_slider->setValue(0.f);

    layout = row(
        Modifier()
            .align(Align::LEFT | Align::TOP)
            .setfixedHeight(48.f)
            .setColor(app->resources.activeTheme->track_color),
    contains{
        scrubber_slider,  
    });

    parentContainer = timeline->getLayout();

    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

bool ScrubberComp::handleEvents() {
    return false;
}

void ScrubberComp::update() {
    if (!scrubber_slider) return;
    
    bool isBeingDragged = (app->ui->m_activeDragSlider == scrubber_slider);
    
    if (isBeingDragged) {
        // Update config from slider value
        if (scrubber_slider->getValue() != lastValue) {
            app->writeConfig<float>("scrubber_position", scrubber_slider->getValue());
            lastValue = scrubber_slider->getValue();
        }
    } else {
        // Update slider from config if not being dragged
        float configValue = app->readConfig<float>("scrubber_position", 0.0f);
        
        if (std::abs(configValue - scrubber_slider->getValue()) > 0.001f) {
            scrubber_slider->setValue(configValue);
            lastValue = configValue;
        }
    }
}

float ScrubberComp::grabValue() {
    return scrubber_slider->getValue();
}

GET_INTERFACE
DECLARE_PLUGIN(ScrubberComp);