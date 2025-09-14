#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <string>
#include <vector>
#include <memory>

#include "Effect.hpp"

// Forward declarations
class AudioClip;

// Base Track class - contains common functionality for all track types
class Track {
public:
    enum class TrackType {
        Audio,
        MIDI
    };

    Track();
    virtual ~Track() = default;

    // Common track properties
    void setName(const std::string& name);
    std::string getName() const;
    void setVolume(float db);
    float getVolume() const;
    void setPan(float pan);
    float getPan() const;
    
    // Common track states
    void toggleMute() { muted = !muted; }
    bool isMuted() const { return muted; }
    void setSolo(bool solo) { soloed = solo; }
    bool isSolo() const { return soloed; }

    // Track type identification
    virtual TrackType getType() const = 0;

    // Audio processing - must be implemented by derived classes
    virtual void process(double playheadSeconds, juce::AudioBuffer<float>& outputBuffer, int numSamples, double sampleRate) = 0;
    virtual void prepareToPlay(double sampleRate, int bufferSize) = 0;

    // Audio clip management - must be implemented by derived classes
    virtual void clearClips() = 0;
    virtual const std::vector<AudioClip>& getClips() const = 0;
    virtual void addClip(const AudioClip& clip) = 0;
    virtual void removeClip(size_t index) = 0;
    virtual AudioClip* getReferenceClip() = 0;

    // Effect management - common to all track types
    Effect* addEffect(const std::string& vstPath);
    bool removeEffect(int index);
    bool removeEffect(const std::string& name);
    Effect* getEffect(int index);
    Effect* getEffect(const std::string& name);
    int getEffectIndex(const std::string& name) const;
    inline std::vector<std::unique_ptr<Effect>>& getEffects() { return effects; }
    int getEffectCount() const { return effects.size(); }
    
    void processEffects(juce::AudioBuffer<float>& buffer);
    void updateEffectEditors() {
        for (auto& effect : effects) {
            if (effect) {
                effect->updateEditor();
            }
        }
    }

    bool moveEffect(int fromIndex, int toIndex);
    void clearEffects();

protected:
    // Common track data
    std::string name;
    float volumeDb = 0.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;

    // Audio processing state
    double currentSampleRate = 44100.0;
    int currentBufferSize = 512;

    // Effects chain
    std::vector<std::unique_ptr<Effect>> effects;

    // Helper method for effects management
    void updateEffectIndices();
};
