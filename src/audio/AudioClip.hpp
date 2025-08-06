#pragma once

#include <juce_core/juce_core.h>

struct AudioClip {
    juce::File sourceFile;
    double startTime;
    double offset;
    double duration;
    float volume;

    AudioClip();
    AudioClip(const juce::File& sourceFile, double startTime, double offset, double duration, float volume = 1.f);
};