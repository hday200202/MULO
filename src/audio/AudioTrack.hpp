#pragma once

#include "Track.hpp"
#include "AudioClip.hpp"

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

// AudioTrack - handles audio clips and audio-specific functionality
class AudioTrack : public Track {
public:
    AudioTrack(juce::AudioFormatManager& formatManager);
    ~AudioTrack() override = default;

    // Track type identification
    TrackType getType() const override { return TrackType::Audio; }

    // Audio clip management
    void addClip(const AudioClip& clip) override;
    void removeClip(size_t index) override;
    const std::vector<AudioClip>& getClips() const override;
    void clearClips() override;
    void setReferenceClip(const AudioClip& clip);
    AudioClip* getReferenceClip() override;

    // Audio processing implementation
    void process(double playheadSeconds, juce::AudioBuffer<float>& outputBuffer, int numSamples, double sampleRate) override;
    void prepareToPlay(double sampleRate, int bufferSize) override;

private:
    // Audio-specific data
    std::vector<AudioClip> clips;
    std::unique_ptr<AudioClip> referenceClip;
    juce::AudioFormatManager& formatManager;
};
