#include "Track.hpp"
#include "../DebugConfig.hpp"
#include <juce_dsp/juce_dsp.h>

Track::Track() {
    // Initialize built-in automation parameters
    float normalizedVolume = volumeSliderToAutomation(decibelsToFloat(volumeDb));
    float normalizedPan = (pan + 1.0f) * 0.5f;
    automationData["Track"]["Volume"].emplace_back(-1.0, normalizedVolume, 0.5f);    
    automationData["Track"]["Pan"].emplace_back(-1.0, normalizedPan, 0.5f);
    
    // Set default potential automation to Volume
    potentialAutomation = {"Track", "Volume"};
    hasActivePotentialAutomation = true;
}

void Track::setName(const std::string& n) { name = n; }
std::string Track::getName() const { return name; }
void Track::setVolume(float db) { 
    if (std::abs(db - volumeDb) > 0.001f) {
        volumeDb = db;

        // Update automation data
        float sliderValue = decibelsToFloat(volumeDb);
        float normalizedVolume = volumeSliderToAutomation(sliderValue);
        
        if (!automationData["Track"]["Volume"].empty()) {
            automationData["Track"]["Volume"][0].value = normalizedVolume;
        } else {
            automationData["Track"]["Volume"].emplace_back(-1.0, normalizedVolume, 0.5f);
        }
        
        potentialAutomation = {"Track", "Volume"};
        hasActivePotentialAutomation = true;
    }
}

float Track::getVolume() const { return volumeDb; }

void Track::setPan(float p) { 
    float newPan = juce::jlimit(-1.f, 1.f, p);
    
    if (std::abs(newPan - pan) > 0.001f) {
        pan = newPan;
        float normalizedPan = (pan + 1.0f) * 0.5f;
                
        if (!automationData["Track"]["Pan"].empty()) {
            automationData["Track"]["Pan"][0].value = normalizedPan;
        } else {
            automationData["Track"]["Pan"].emplace_back(-1.0, normalizedPan, 0.5f);
        }
        
        potentialAutomation = {"Track", "Pan"};
        hasActivePotentialAutomation = true;
    }
}
float Track::getPan() const { return pan; }

Effect* Track::addEffect(const std::string& vstPath) {
    auto effect = std::make_unique<Effect>();
    if (effect->loadVST(vstPath)) {
        if (currentSampleRate > 0 && currentBufferSize > 0) {
            effect->prepareToPlay(currentSampleRate, currentBufferSize);
        }
        
        effects.push_back(std::move(effect));
        updateEffectIndices();
        
        Effect* addedEffect = effects.back().get();
        
        if (addedEffect && addedEffect->isSynthesizer()) {
        }

        // All Parameters
        const auto params = addedEffect->getAllParameters();
        int i = 0;
        for (const auto param : params) {
            std::string name = param->getName(256).toStdString();
            
            if (name.find("CC") != std::string::npos) // Filter out any parameter containing "CC"
                continue;

            float value = param->getValue();
            std::string effectKey = addedEffect->getName() + "_" + std::to_string(effects.size() - 1);
            automationData[effectKey][name].emplace_back(-1.0, value, 0.5f);
            if (i == addedEffect->getNumParameters() - 1)
                break;

            ++i;
        }
        
        return addedEffect;
    }
    return nullptr;
}

bool Track::removeEffect(int index) {
    if (index >= 0 && index < effects.size()) {
        effects.erase(effects.begin() + index);
        updateEffectIndices();
        return true;
    }
    return false;
}

bool Track::removeEffect(const std::string& name) {
    for (auto it = effects.begin(); it != effects.end(); ++it) {
        if ((*it)->getName() == name) {
            effects.erase(it);
            updateEffectIndices();
            return true;
        }
    }
    return false;
}

Effect* Track::getEffect(int index) {
    if (index >= 0 && index < effects.size()) {
        return effects[index].get();
    }
    return nullptr;
}

Effect* Track::getEffect(const std::string& name) {
    for (auto& effect : effects) {
        if (effect && effect->getName() == name) {
            return effect.get();
        }
    }
    return nullptr;
}

int Track::getEffectIndex(const std::string& name) const {
    for (size_t i = 0; i < effects.size(); ++i) {
        if (effects[i] && effects[i]->getName() == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Track::processEffects(juce::AudioBuffer<float>& buffer) {
    for (const auto& effect : effects) {
        if (effect && effect->enabled()) {
            effect->processAudio(buffer);
        }
    }
}

bool Track::moveEffect(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= effects.size() || 
        toIndex < 0 || toIndex >= effects.size() || 
        fromIndex == toIndex) {
        return false;
    }
    
    std::unique_ptr<Effect> effect = std::move(effects[fromIndex]);
    effects.erase(effects.begin() + fromIndex);
    effects.insert(effects.begin() + toIndex, std::move(effect));
    updateEffectIndices();
    
    return true;
}

void Track::clearEffects() {
    effects.clear();
}

void Track::updateEffectIndices() {
    for (size_t i = 0; i < effects.size(); ++i) {
        if (effects[i]) {
            effects[i]->setIndex(static_cast<int>(i));
        }
    }
}

void Track::updateParameterTracking() {
    for (size_t i = 0; i < effects.size(); ++i) {
        const auto& effect = effects[i];
        if (!effect) continue;
        
        std::string effectKey = effect->getName() + "_" + std::to_string(i);
        auto params = effect->getAllParameters();
        
        for (int p = 0; p < params.size(); ++p) {
            auto* param = params[p];
            if (!param) continue;
            
            std::string paramName = param->getName(256).toStdString();
            float currentValue = param->getValue();
            
            // Check if this is the first time we're tracking this parameter
            if (lastParameterValues[effectKey].find(paramName) == lastParameterValues[effectKey].end()) {
                lastParameterValues[effectKey][paramName] = currentValue;
                continue;
            }
            
            // Check if parameter value has changed
            float lastValue = lastParameterValues[effectKey][paramName];
            if (std::abs(currentValue - lastValue) > 0.001f) {
                potentialAutomation = {effectKey, paramName};
                hasActivePotentialAutomation = true;
                lastParameterValues[effectKey][paramName] = currentValue;
                return;
            }
        }
    }
}

float Track::getCurrentParameterValue(const std::string& effectName, const std::string& parameterName) const {
    // Handle built-in track parameters
    if (effectName == "Track") {
        if (parameterName == "Volume") {
            float sliderValue = decibelsToFloat(volumeDb);
            return volumeSliderToAutomation(sliderValue);
        } else if (parameterName == "Pan") {
            return (pan + 1.0f) * 0.5f;
        }
    } else {
        // Handle effect parameters
        for (size_t i = 0; i < effects.size(); ++i) {
            const auto& effect = effects[i];
            std::string effectKey = effect->getName() + "_" + std::to_string(i);
            
            if (effectKey == effectName) {
                auto params = effect->getAllParameters();
                for (int p = 0; p < params.size(); ++p) {
                    auto* param = params[p];
                    if (param && param->getName(256).toStdString() == parameterName) {
                        return param->getValue();
                    }
                }
                break;
            }
        }
    }
    
    // Fallback if live parameter not found
    auto effectIt = automationData.find(effectName);
    if (effectIt != automationData.end()) {
        auto paramIt = effectIt->second.find(parameterName);
        if (paramIt != effectIt->second.end() && !paramIt->second.empty()) {
            return paramIt->second.front().value;
        }
    }
    
    return 0.5f;
}

const std::pair<std::string, std::string>& Track::getPotentialAutomation() const {
    return potentialAutomation;
}

void Track::setPotentialAutomation(const std::string& effectName, const std::string& parameterName) {
    potentialAutomation = {effectName, parameterName};
    hasActivePotentialAutomation = true;
}

void Track::clearPotentialAutomation() {
    potentialAutomation = {"", ""};
    hasActivePotentialAutomation = false;
}

bool Track::hasPotentialAutomation() const {
    return hasActivePotentialAutomation || (!potentialAutomation.first.empty() && !potentialAutomation.second.empty());
}
