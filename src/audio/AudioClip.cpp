#include "AudioClip.hpp"

AudioClip::AudioClip()
: startTime(0.0), offset(0.0), duration(0.0), volume(1.0f) {}

AudioClip::AudioClip(
    const juce::File& sourceFile, 
    double startTime, 
    double offset, 
    double duration, 
    float volume
) : sourceFile(sourceFile), startTime(startTime), offset(offset), duration(duration), volume(volume) {}