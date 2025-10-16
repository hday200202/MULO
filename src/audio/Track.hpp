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
    virtual AudioClip* getClip(size_t index) { return nullptr; } // Default implementation for MIDI tracks
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

    // Automation
    inline void addAutomationPoint(
        const std::string& effectName, 
        const std::string& parameterName, 
        const AutomationPoint& automationPoint
    ) {
        automationData[effectName][parameterName].push_back(automationPoint);
        
        std::pair<std::string, std::string> paramPair = {effectName, parameterName};
        if (std::find(automatedParameters.begin(), automatedParameters.end(), paramPair) == automatedParameters.end()) {
            automatedParameters.push_back(paramPair);
        }
    }
    
    inline const std::unordered_map<std::string, std::unordered_map<std::string, std::vector<AutomationPoint>>>& getAutomationData() const {
        return automationData;
    }
    
    inline const std::vector<std::pair<std::string, std::string>>& getAutomatedParameters() const {
        return automatedParameters;
    }
    
    inline const std::vector<AutomationPoint>* getAutomationPoints(const std::string& effectName, const std::string& parameterName) const {
        auto effectIt = automationData.find(effectName);
        if (effectIt != automationData.end()) {
            auto paramIt = effectIt->second.find(parameterName);
            if (paramIt != effectIt->second.end()) {
                return &paramIt->second;
            }
        }
        return nullptr;
    }

    inline bool removeAutomationPoint(const std::string& effectName, const std::string& parameterName, float time, float tolerance = 0.001f) {
        auto effectIt = automationData.find(effectName);
        if (effectIt != automationData.end()) {
            auto paramIt = effectIt->second.find(parameterName);
            if (paramIt != effectIt->second.end()) {
                auto& points = paramIt->second;
                auto it = std::find_if(points.begin(), points.end(), 
                    [time, tolerance](const AutomationPoint& point) {
                        return std::abs(point.time - time) < tolerance;
                    });
                if (it != points.end()) {
                    points.erase(it);
                    
                    // Remove from automation list if less than 2 points
                    if (points.size() < 2) {
                        std::pair<std::string, std::string> paramPair = {effectName, parameterName};
                        automatedParameters.erase(
                            std::remove(automatedParameters.begin(), automatedParameters.end(), paramPair),
                            automatedParameters.end()
                        );
                        
                        if (points.empty()) {
                            effectIt->second.erase(paramIt);
                            if (effectIt->second.empty()) {
                                automationData.erase(effectIt);
                            }
                        }
                    }
                    return true;
                }
            }
        }
        return false;
    }

    inline bool moveAutomationPoint(const std::string& effectName, const std::string& parameterName, 
                                   float oldTime, float newTime, float newValue, float tolerance = 0.001f) {
        auto effectIt = automationData.find(effectName);
        if (effectIt != automationData.end()) {
            auto paramIt = effectIt->second.find(parameterName);
            if (paramIt != effectIt->second.end()) {
                auto& points = paramIt->second;
                auto it = std::find_if(points.begin(), points.end(), 
                    [oldTime, tolerance](const AutomationPoint& point) {
                        return std::abs(point.time - oldTime) < tolerance;
                    });
                if (it != points.end()) {
                    it->time = newTime;
                    it->value = std::max(0.0f, std::min(1.0f, newValue));
                    return true;
                }
            }
        }
        return false;
    }

    // More precise point moving that matches both time and value
    inline bool moveAutomationPointPrecise(const std::string& effectName, const std::string& parameterName, 
                                          float oldTime, float oldValue, float newTime, float newValue, 
                                          float timeTolerance = 0.0001f, float valueTolerance = 0.001f) {
        auto effectIt = automationData.find(effectName);
        if (effectIt != automationData.end()) {
            auto paramIt = effectIt->second.find(parameterName);
            if (paramIt != effectIt->second.end()) {
                auto& points = paramIt->second;
                auto it = std::find_if(points.begin(), points.end(), 
                    [oldTime, oldValue, timeTolerance, valueTolerance](const AutomationPoint& point) {
                        return std::abs(point.time - oldTime) < timeTolerance && 
                               std::abs(point.value - oldValue) < valueTolerance;
                    });
                if (it != points.end()) {
                    it->time = newTime;
                    it->value = std::max(0.0f, std::min(1.0f, newValue));
                    return true;
                }
            }
        }
        return false;
    }

    // Precise point removal that matches both time and value
    inline bool removeAutomationPointPrecise(const std::string& effectName, const std::string& parameterName, 
                                            float time, float value, 
                                            float timeTolerance = 0.0001f, float valueTolerance = 0.001f) {
        auto effectIt = automationData.find(effectName);
        if (effectIt != automationData.end()) {
            auto paramIt = effectIt->second.find(parameterName);
            if (paramIt != effectIt->second.end()) {
                auto& points = paramIt->second;
                auto it = std::find_if(points.begin(), points.end(), 
                    [time, value, timeTolerance, valueTolerance](const AutomationPoint& point) {
                        return std::abs(point.time - time) < timeTolerance && 
                               std::abs(point.value - value) < valueTolerance;
                    });
                if (it != points.end()) {
                    points.erase(it);
                    
                    // Remove from automation list if less than 2 points
                    if (points.size() < 2) {
                        std::pair<std::string, std::string> paramPair = {effectName, parameterName};
                        automatedParameters.erase(
                            std::remove(automatedParameters.begin(), automatedParameters.end(), paramPair),
                            automatedParameters.end()
                        );
                    }
                    return true;
                }
            }
        }
        return false;
    }

    inline bool updateAutomationPointCurve(const std::string& effectName, const std::string& parameterName, 
                                          float time, float newCurve, float tolerance = 0.001f) {
        auto effectIt = automationData.find(effectName);
        if (effectIt != automationData.end()) {
            auto paramIt = effectIt->second.find(parameterName);
            if (paramIt != effectIt->second.end()) {
                auto& points = paramIt->second;
                auto it = std::find_if(points.begin(), points.end(), 
                    [time, tolerance](const AutomationPoint& point) {
                        return std::abs(point.time - time) < tolerance;
                    });
                if (it != points.end()) {
                    it->curve = std::max(0.0f, std::min(1.0f, newCurve));
                    return true;
                }
            }
        }
        return false;
    }

    inline bool updateAutomationPointCurvePrecise(const std::string& effectName, const std::string& parameterName, 
                                                 float time, float value, float newCurve,
                                                 float timeTolerance = 0.0001f, float valueTolerance = 0.001f) {
        auto effectIt = automationData.find(effectName);
        if (effectIt != automationData.end()) {
            auto paramIt = effectIt->second.find(parameterName);
            if (paramIt != effectIt->second.end()) {
                auto& points = paramIt->second;
                auto it = std::find_if(points.begin(), points.end(), 
                    [time, value, timeTolerance, valueTolerance](const AutomationPoint& point) {
                        return std::abs(point.time - time) < timeTolerance && 
                               std::abs(point.value - value) < valueTolerance;
                    });
                if (it != points.end()) {
                    it->curve = std::max(0.0f, std::min(1.0f, newCurve));
                    return true;
                }
            }
        }
        return false;
    }

    inline bool clearAutomationParameter(const std::string& effectName, const std::string& parameterName) {
        auto effectIt = automationData.find(effectName);
        if (effectIt != automationData.end()) {
            auto paramIt = effectIt->second.find(parameterName);
            if (paramIt != effectIt->second.end()) {
                std::pair<std::string, std::string> paramPair = {effectName, parameterName};
                automatedParameters.erase(
                    std::remove(automatedParameters.begin(), automatedParameters.end(), paramPair),
                    automatedParameters.end()
                );
                
                effectIt->second.erase(paramIt);
                if (effectIt->second.empty()) {
                    automationData.erase(effectIt);
                }
                
                return true;
            }
        }
        return false;
    }

    // Parameter change detection for automation
    void updateParameterTracking();
    const std::pair<std::string, std::string>& getPotentialAutomation() const;
    void setPotentialAutomation(const std::string& effectName, const std::string& parameterName);
    void clearPotentialAutomation();
    bool hasPotentialAutomation() const;
    
    float getCurrentParameterValue(const std::string& effectName, const std::string& parameterName) const;

    inline void applyAutomation(double positionSeconds) {
        auto getInterpolatedValue = [](const std::vector<AutomationPoint>& points, double time) -> float {
            if (points.empty()) return -1.0f;
            
            std::vector<const AutomationPoint*> timelinePoints;
            for (const auto& point : points) {
                if (point.time >= 0.0) {
                    timelinePoints.push_back(&point);
                }
            }
            
            if (timelinePoints.empty()) return -1.0f;
            
            std::sort(timelinePoints.begin(), timelinePoints.end(), 
                [](const AutomationPoint* a, const AutomationPoint* b) {
                    return a->time < b->time;
                });
            
            if (time <= timelinePoints[0]->time) {
                return timelinePoints[0]->value;
            }
            
            if (time >= timelinePoints.back()->time) {
                return timelinePoints.back()->value;
            }
            
            for (size_t i = 0; i < timelinePoints.size() - 1; ++i) {
                const auto* p1 = timelinePoints[i];
                const auto* p2 = timelinePoints[i + 1];
                
                if (time >= p1->time && time <= p2->time) {
                    double t = (time - p1->time) / (p2->time - p1->time);
                    
                    if (std::abs(p1->curve - 0.5f) < 0.001f) {
                        return p1->value + t * (p2->value - p1->value);
                    } else {
                        float curve = p1->curve;
                        float adjustedT;
                        
                        if (curve < 0.5f) {
                            // Ease-in: slow start, fast end
                            // Ultra-sharp at 0.0f (almost perfect corner)
                            float factor = 50.0f * (0.5f - curve); // 0.0f->25.0f, 0.5f->0.0f
                            adjustedT = std::pow(t, 1.0f + factor);
                        } else {
                            // Ease-out: fast start, slow end  
                            // Ultra-sharp at 1.0f (almost perfect corner)
                            float factor = 50.0f * (curve - 0.5f); // 0.5f->0.0f, 1.0f->25.0f
                            adjustedT = 1.0f - std::pow(1.0f - t, 1.0f + factor);
                        }
                        
                        return p1->value + adjustedT * (p2->value - p1->value);
                    }
                }
            }
            
            return -1.0f;
        };
        
        auto trackIt = automationData.find("Track");
        if (trackIt != automationData.end()) {
            auto& trackParams = trackIt->second;
            
            auto volumeIt = trackParams.find("Volume");
            if (volumeIt != trackParams.end() && !volumeIt->second.empty()) {
                float automatedValue = getInterpolatedValue(volumeIt->second, positionSeconds);
                if (automatedValue >= 0.0f) {
                    float dbValue = automationToVolumeSlider(automatedValue);
                    volumeDb = floatToDecibels(dbValue);
                }
            }
            
            // Apply Pan automation
            auto panIt = trackParams.find("Pan");
            if (panIt != trackParams.end() && !panIt->second.empty()) {
                float automatedValue = getInterpolatedValue(panIt->second, positionSeconds);
                if (automatedValue >= 0.0f) {
                    pan = (automatedValue * 2.0f) - 1.0f;
                    pan = juce::jlimit(-1.0f, 1.0f, pan);
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
                    
                    std::string paramName = ap->getName(256).toStdString();
                    auto pit = paramMap.find(paramName);
                    if (pit != paramMap.end() && !pit->second.empty()) {
                        float automatedValue = getInterpolatedValue(pit->second, positionSeconds);
                        if (automatedValue >= 0.0f) {
                            effect->setParameter(p, automatedValue);
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
    
    // List of automated parameters in the order they were automated
    std::vector<std::pair<std::string, std::string>> automatedParameters;
    
    // Parameter change detection for automation
    std::pair<std::string, std::string> potentialAutomation;
    std::unordered_map<std::string, std::unordered_map<std::string, float>> lastParameterValues;
    bool hasActivePotentialAutomation = false;

    // Helper method for effects management
    void updateEffectIndices();
    
    // Parameter change detection
    void detectParameterChanges();
};
