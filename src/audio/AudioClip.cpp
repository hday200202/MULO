#include "AudioClip.hpp"

AudioClip::AudioClip() : startTime(0.0), offset(0.0), duration(0.0), volume(1.0f) {}

AudioClip::AudioClip(const juce::File& sourceFile, double startTime, double offset, double duration, float volume)
    : sourceFile(sourceFile), startTime(startTime), offset(offset), duration(duration), volume(volume) {}

AudioClip::AudioClip(const AudioClip& other) 
    : sourceFile(juce::File(other.sourceFile.getFullPathName())),
      startTime(other.startTime), 
      offset(other.offset), 
      duration(other.duration), 
      volume(other.volume) {}

AudioClip& AudioClip::operator=(const AudioClip& other) {
    if (this != &other) {
        sourceFile = juce::File(other.sourceFile.getFullPathName());
        startTime = other.startTime;
        offset = other.offset;
        duration = other.duration;
        volume = other.volume;
    }
    return *this;
}