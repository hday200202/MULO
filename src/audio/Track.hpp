
#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "AudioClip.hpp"
#include "Effect.hpp"

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
    
    void prepareToPlay(double sampleRate, int bufferSize);

    void toggleMute() { muted = !muted; }
    bool isMuted() const { return muted; }
    void setSolo(bool solo) { soloed = solo; }
    bool isSolo() const { return soloed; }

    Effect* addEffect(const std::string& vstPath);
    bool removeEffect(int index);
    bool removeEffect(const std::string& name);
    Effect* getEffect(int index);
    Effect* getEffect(const std::string& name);
    int getEffectIndex(const std::string& name) const;
    inline std::vector<std::unique_ptr<Effect>>& getEffects() { return effects; }
    int getEffectCount() const { return effects.size(); }

    void processEffects(juce::AudioBuffer<float>& buffer);

    bool moveEffect(int fromIndex, int toIndex);
    void clearEffects();

private:
    void updateEffectIndices(); // Helper to update effect indices after changes
    std::string name;
    float volumeDb = 0.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;

    std::vector<AudioClip> clips;
    std::unique_ptr<AudioClip> referenceClip;

    juce::AudioFormatManager& formatManager;

    // Audio settings for effects
    double currentSampleRate = 44100.0;
    int currentBufferSize = 512;

    std::vector<std::unique_ptr<Effect>> effects;
};