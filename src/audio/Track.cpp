
#include "Track.hpp"
#include "../DebugConfig.hpp"
#include "../DebugConfig.hpp"

Track::Track(juce::AudioFormatManager& fm) : formatManager(fm) {}
Track::~Track() {} // unique_ptr handles referenceClip deletion

void Track::setName(const std::string& n) { name = n; }
std::string Track::getName() const { return name; }
void Track::setVolume(float db) { volumeDb = db; }
float Track::getVolume() const { return volumeDb; }
void Track::setPan(float p) { pan = juce::jlimit(-1.f, 1.f, p); }
float Track::getPan() const { return pan; }
void Track::addClip(const AudioClip& c) { clips.push_back(c); }
void Track::removeClip(int idx) {
    if (idx >= 0 && idx < clips.size()) {
        clips.erase(clips.begin() + idx);
    }
}
const std::vector<AudioClip>& Track::getClips() const { return clips; }

void Track::setReferenceClip(const AudioClip& clip) {
    referenceClip = std::make_unique<AudioClip>(clip);
}

AudioClip* Track::getReferenceClip() {
    return referenceClip.get();
}

void Track::process(double playheadSeconds,
                    juce::AudioBuffer<float>& output,
                    int numSamples,
                    double sampleRate) {
    const AudioClip* active = nullptr;

    for (const auto& c : clips) {
        if (playheadSeconds < c.startTime + c.duration &&
            playheadSeconds + (double)numSamples / sampleRate > c.startTime)
        {
            active = &c;
            break;
        }
    }

    if (!active)
        return;

    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        formatManager.createReaderFor(active->sourceFile));

    if (!reader) {
        DBG("Failed to create reader for: " << active->sourceFile.getFullPathName());
        return;
    }

    double blockStartTimeSeconds = playheadSeconds;
    double blockEndTimeSeconds = playheadSeconds + (double)numSamples / sampleRate;

    double readStartTimeInClip = juce::jmax(0.0, blockStartTimeSeconds - active->startTime);
    double readEndTimeInClip = juce::jmin(active->duration, blockEndTimeSeconds - active->startTime);

    if (readStartTimeInClip >= active->duration || readEndTimeInClip <= 0.0) {
        return;
    }

    juce::int64 sourceFileStartSample = static_cast<juce::int64>((active->offset + readStartTimeInClip) * reader->sampleRate);
    juce::int64 sourceFileEndSample = static_cast<juce::int64>((active->offset + readEndTimeInClip) * reader->sampleRate);
    juce::int64 numSamplesToRead = sourceFileEndSample - sourceFileStartSample;

    juce::int64 outputBufferStartSample = static_cast<juce::int64>(juce::jmax(0.0, (active->startTime - blockStartTimeSeconds) * sampleRate));
    juce::int64 numSamplesToWrite = juce::jmin((juce::int64)numSamples - outputBufferStartSample, numSamplesToRead);
    
    if (numSamplesToRead <= 0 || numSamplesToWrite <= 0) {
        return;
    }
    
    outputBufferStartSample = juce::jmax((juce::int64)0, outputBufferStartSample);

    juce::AudioBuffer<float> tempClipBuf(reader->numChannels, (int)numSamplesToRead);
    tempClipBuf.clear();
    reader->read(&tempClipBuf, 0, (int)numSamplesToRead, sourceFileStartSample, true, true);

    float gain = juce::Decibels::decibelsToGain(volumeDb) * active->volume;

    for (int ch = 0; ch < output.getNumChannels(); ++ch) {
        float panL = (1.0f - juce::jlimit(-1.f, 1.f, pan)) * 0.5f;
        float panR = (1.0f + juce::jlimit(-1.f, 1.f, pan)) * 0.5f;
        float panGain = (ch == 0) ? panL : panR;
        
        output.addFrom(ch, (int)outputBufferStartSample,
                       tempClipBuf, ch % tempClipBuf.getNumChannels(),
                       0, (int)numSamplesToWrite,
                       gain * panGain);
    }
    
    // Don't apply effects here - they should be applied after mixing
    // processEffects(output);
}

void Track::prepareToPlay(double sampleRate, int bufferSize) {
    currentSampleRate = sampleRate;
    currentBufferSize = bufferSize;
    
    // Prepare all existing effects with new audio settings
    for (auto& effect : effects) {
        if (effect) {
            effect->prepareToPlay(sampleRate, bufferSize);
        }
    }
    
    DEBUG_PRINT("Track '" << name << "' prepared for playback: " 
              << sampleRate << " Hz, " << bufferSize << " samples");
}

Effect* Track::addEffect(const std::string& vstPath) {
    auto effect = std::make_unique<Effect>();
    
    if (!effect->loadVST(vstPath)) {
        std::cerr << "Failed to load VST: " << vstPath << std::endl;
        return nullptr;
    }
    
    // Prepare the effect with current audio settings
    effect->prepareToPlay(currentSampleRate, currentBufferSize);
    
    Effect* effectPtr = effect.get();
    effects.push_back(std::move(effect));
    
    // Update all effect indices after adding
    updateEffectIndices();
    
    // Note: VST window opening is now handled by the UI components (FXRack, etc.)
    // This prevents conflicts between auto-opening and user-triggered opening
    
    DEBUG_PRINT("Added effect '" << effectPtr->getName() << "' to track '" << name << "'");
    return effectPtr;
}

bool Track::removeEffect(int index) {
    if (index < 0 || index >= static_cast<int>(effects.size())) {
        return false;
    }
    
    std::string effectName = effects[index]->getName();
    effects.erase(effects.begin() + index);
    
    // Update all effect indices after removal
    updateEffectIndices();
    
    DEBUG_PRINT("Removed effect '" << effectName << "' from track '" << name << "'");
    return true;
}

bool Track::removeEffect(const std::string& name) {
    for (size_t i = 0; i < effects.size(); ++i) {
        if (effects[i]->getName() == name) {
            return removeEffect(static_cast<int>(i));
        }
    }
    return false;
}

Effect* Track::getEffect(int index) {
    if (index < 0 || index >= static_cast<int>(effects.size())) {
        return nullptr;
    }
    return effects[index].get();
}

Effect* Track::getEffect(const std::string& name) {
    for (auto& effect : effects) {
        if (effect->getName() == name) {
            return effect.get();
        }
    }
    return nullptr;
}

void Track::processEffects(juce::AudioBuffer<float>& buffer) {
    // Apply each effect in the chain sequentially
    for (auto& effect : effects) {
        if (effect && effect->enabled()) {
            effect->processAudio(buffer);
        }
    }
}

bool Track::moveEffect(int fromIndex, int toIndex) {
    if (fromIndex < 0 || fromIndex >= static_cast<int>(effects.size()) ||
        toIndex < 0 || toIndex >= static_cast<int>(effects.size()) ||
        fromIndex == toIndex) {
        return false;
    }
    
    // Move the effect
    auto effect = std::move(effects[fromIndex]);
    effects.erase(effects.begin() + fromIndex);
    
    // Adjust toIndex if we removed an element before it
    if (fromIndex < toIndex) {
        toIndex--;
    }
    
    effects.insert(effects.begin() + toIndex, std::move(effect));
    
    // Update all effect indices after moving
    updateEffectIndices();
    
    DEBUG_PRINT("Moved effect from position " << fromIndex << " to " << toIndex 
              << " in track '" << name << "'");
    return true;
}

void Track::clearEffects() {
    size_t numEffects = effects.size();
    effects.clear();
    
    if (numEffects > 0) {
        DEBUG_PRINT("Cleared " << numEffects << " effects from track '" << name << "'");
    }
}

void Track::updateEffectIndices() {
    for (size_t i = 0; i < effects.size(); ++i) {
        if (effects[i]) {
            effects[i]->setIndex(static_cast<int>(i));
        }
    }
}