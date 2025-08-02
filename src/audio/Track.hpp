
#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "AudioClip.hpp"

class Track {
public:
    Track(juce::AudioFormatManager& formatManager);
    
    ~Track();

    void setName(const std::string& name);
    
    std::string getName() const;

    void setVolume(float db);
    
    float getVolume() const;

    void setPan(float pan);
    
    float getPan() const;

    void addClip(const AudioClip& clip);
    
    void removeClip(int index);
    
    const std::vector<AudioClip>& getClips() const;

    void setReferenceClip(const AudioClip& clip);
    
    AudioClip* getReferenceClip();

    void process(
        double playheadSeconds,
        juce::AudioBuffer<float>& outputBuffer,
        int numSamples,
        double sampleRate
    );

    void toggleMute() { muted = !muted; }
    
    bool isMuted() const { return muted; }

    void setSolo(bool solo) { soloed = solo; }
    
    bool isSolo() const { return soloed; }

private:
    std::string name;
    float volumeDb = 0.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;

    std::vector<AudioClip> clips;
    std::unique_ptr<AudioClip> referenceClip;

    juce::AudioFormatManager& formatManager;
};