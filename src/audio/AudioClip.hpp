#pragma once

#include <juce_core/juce_core.h>

/**
 * @brief Represents a single audio clip on a track.
 */
struct AudioClip {
    juce::File sourceFile;
    double startTime;
    double offset;
    double duration;
    float volume;

    AudioClip();
    AudioClip(
        const juce::File& sourceFile, 
        double startTime, 
        double offset, 
        double duration, 
        float volume = 1.f
    );
};