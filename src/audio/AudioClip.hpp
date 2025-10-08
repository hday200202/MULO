#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>

struct AudioClip {
    juce::File sourceFile;
    double startTime;
    double offset;
    double duration;
    float volume;
    
    mutable std::unique_ptr<juce::AudioFormatReader> cachedReader;
    mutable std::unique_ptr<juce::AudioBuffer<float>> preRenderedAudio;
    mutable double cachedSampleRate = 0.0;
    mutable bool isLoaded = false;

    AudioClip();
    AudioClip(const juce::File& sourceFile, double startTime, double offset, double duration, float volume = 1.f);
    AudioClip(const AudioClip& other);
    AudioClip& operator=(const AudioClip& other);
    
    // Cache management
    void loadAudioData(juce::AudioFormatManager& formatManager, double targetSampleRate) const;
    void unloadAudioData() const;
    bool isAudioDataLoaded() const { return isLoaded && preRenderedAudio != nullptr; }
};