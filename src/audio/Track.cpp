#include "Track.hpp"
#include "../DebugConfig.hpp"
#include <juce_dsp/juce_dsp.h>

Track::Track() {}

void Track::setName(const std::string& n) { name = n; }
std::string Track::getName() const { return name; }
void Track::setVolume(float db) { volumeDb = db; }
float Track::getVolume() const { return volumeDb; }
void Track::setPan(float p) { pan = juce::jlimit(-1.f, 1.f, p); }
float Track::getPan() const { return pan; }

Effect* Track::addEffect(const std::string& vstPath) {
    auto effect = std::make_unique<Effect>();
    if (effect->loadVST(vstPath)) {
        effects.push_back(std::move(effect));
        return effects.back().get();
    }
    return nullptr;
}

bool Track::removeEffect(int index) {
    if (index >= 0 && index < effects.size()) {
        effects.erase(effects.begin() + index);
        updateEffectIndices();
        DEBUG_PRINT("Removed effect at index " << index << " from track " << name);
        return true;
    }
    return false;
}

bool Track::removeEffect(const std::string& name) {
    for (size_t i = 0; i < effects.size(); ++i) {
        if (effects[i] && effects[i]->getName() == name) {
            effects.erase(effects.begin() + i);
            updateEffectIndices();
            DEBUG_PRINT("Removed effect " << name << " from track " << this->name);
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
    
    DEBUG_PRINT("Moved effect from index " << fromIndex << " to " << toIndex << " on track " << name);
    return true;
}

void Track::clearEffects() {
    effects.clear();
    DEBUG_PRINT("Cleared all effects from track " << name);
}

void Track::updateEffectIndices() {
    for (size_t i = 0; i < effects.size(); ++i) {
        if (effects[i]) {
            effects[i]->setIndex(static_cast<int>(i));
        }
    }
}
