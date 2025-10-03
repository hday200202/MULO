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

    struct AutomationPoint {
        double time;
        float value;
        float curve;
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

    // Automation system
    inline void addAutomationPoint(
        const std::string& effectName, 
        const std::string& parameterName, 
        const AutomationPoint& automationPoint
    ) {
        automationData[effectName][parameterName].push_back(automationPoint);
    }

    // Parameter change detection for automation
    void updateParameterTracking();
    const std::pair<std::string, std::string>& getPotentialAutomation() const;
    void clearPotentialAutomation();
    bool hasPotentialAutomation() const;

    inline void applyAutomation(double positionSeconds) {
        // for any parameter that is automated, apply the 0.f - 1.f level to that parameter
        // for the current position in engine.

        // TODO volume at position

        // TODO pan at position

        for (size_t i = 0; i < effects.size(); ++i) {
            const auto& effect = effects[i];
            // Apply automation values to effect parameters
            auto params = effect->getAllParameters();
            std::string effectKey = effect->getName() + "_" + std::to_string(i);
            auto it = automationData.find(effectKey);
            if (it != automationData.end()) {
                auto& paramMap = it->second;
                for (int p = 0; p < params.size(); ++p) {
                    auto* ap = params[p];
                    if (!ap) continue;
                    std::string name = ap->getName(256).toStdString();
                    auto pit = paramMap.find(name);
                    if (pit != paramMap.end() && !pit->second.empty()) {
                        // use first automation point value for now
                        float val = pit->second.front().value;
                        effect->setParameter(p, val);
                    }
                }
            }
        }
    }

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

    // Effect name -> parameter name -> automation points
    std::unordered_map<std::string, std::unordered_map<std::string, std::vector<AutomationPoint>>> automationData;
    
    // Parameter change detection for automation
    std::pair<std::string, std::string> potentialAutomation;
    std::unordered_map<std::string, std::unordered_map<std::string, float>> lastParameterValues;
    bool hasActivePotentialAutomation = false;

    // Helper method for effects management
    void updateEffectIndices();
    
    // Parameter change detection
    void detectParameterChanges();
};

inline float floatToDecibels(float linear, float minusInfinityDb = -100.0f) {
    constexpr double reference = 0.75;
    if (linear <= 0.0)
        return minusInfinityDb;
    return static_cast<float>(20.0 * std::log10(static_cast<double>(linear) / reference));
}

inline float decibelsToFloat(float db, float minusInfinityDb = -100.0f) {
    constexpr double reference = 0.75;
    if (db <= minusInfinityDb)
        return 0.0f;
    return static_cast<float>(reference * std::pow(10.0, static_cast<double>(db) / 20.0));
}
