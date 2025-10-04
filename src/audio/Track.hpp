#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <string>
#include <vector>
#include <memory>
#include <limits>

#include "Effect.hpp"

class AudioClip;

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

inline float volumeSliderToAutomation(float sliderValue, float minusInfinityDb = -100.0f, float maxDb = 6.0f) {
    return juce::jlimit(0.0f, 1.0f, sliderValue);
}

inline float automationToVolumeSlider(float automationValue, float minusInfinityDb = -100.0f, float maxDb = 6.0f) {
    return juce::jlimit(0.0f, 1.0f, automationValue);
}

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
    
    float getCurrentParameterValue(const std::string& effectName, const std::string& parameterName) const;

    inline void applyAutomation(double positionSeconds) {
        // Apply Track parameter automation only if there are points other than the default point
        auto trackIt = automationData.find("Track");
        if (trackIt != automationData.end()) {
            auto& trackParams = trackIt->second;
            
            auto volumeIt = trackParams.find("Volume");
            if (volumeIt != trackParams.end() && !volumeIt->second.empty()) {
                bool hasTimelineAutomation = false;
                for (const auto& point : volumeIt->second) {
                    if (point.time >= 0.0) {
                        hasTimelineAutomation = true;
                        break;
                    }
                }
                
                if (hasTimelineAutomation) {
                    float targetValue = -1.0f;
                    float minDistance = std::numeric_limits<float>::max();
                    
                    for (const auto& point : volumeIt->second) {
                        if (point.time >= 0.0) {
                            float distance = std::abs(point.time - positionSeconds);
                            if (distance < minDistance) {
                                minDistance = distance;
                                targetValue = point.value;
                            }
                        }
                    }
                    
                    if (targetValue >= 0.0f) {
                        float dbValue = automationToVolumeSlider(targetValue);
                        volumeDb = floatToDecibels(dbValue);
                    }
                }
            }
            
            auto panIt = trackParams.find("Pan");
            if (panIt != trackParams.end() && !panIt->second.empty()) {
                bool hasTimelineAutomation = false;
                for (const auto& point : panIt->second) {
                    if (point.time >= 0.0) {
                        hasTimelineAutomation = true;
                        break;
                    }
                }
                
                if (hasTimelineAutomation) {
                    float targetValue = -1.0f;
                    float minDistance = std::numeric_limits<float>::max();
                    
                    for (const auto& point : panIt->second) {
                        if (point.time >= 0.0) {
                            float distance = std::abs(point.time - positionSeconds);
                            if (distance < minDistance) {
                                minDistance = distance;
                                targetValue = point.value;
                            }
                        }
                    }
                    
                    if (targetValue >= 0.0f) {
                        pan = (targetValue * 2.0f) - 1.0f;
                        pan = juce::jlimit(-1.0f, 1.0f, pan);
                    }
                }
            }
        }

        for (size_t i = 0; i < effects.size(); ++i) {
            const auto& effect = effects[i];
            if (!effect) continue;
            
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
                        bool hasTimelineAutomation = false;
                        for (const auto& point : pit->second) {
                            if (point.time >= 0.0) {
                                hasTimelineAutomation = true;
                                break;
                            }
                        }
                        
                        if (hasTimelineAutomation) {
                            float targetValue = -1.0f;
                            float minDistance = std::numeric_limits<float>::max();
                            
                            for (const auto& point : pit->second) {
                                if (point.time >= 0.0) {
                                    float distance = std::abs(point.time - positionSeconds);
                                    if (distance < minDistance) {
                                        minDistance = distance;
                                        targetValue = point.value;
                                    }
                                }
                            }
                            
                            if (targetValue >= 0.0f) {
                                effect->setParameter(p, targetValue);
                            }
                        }
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
