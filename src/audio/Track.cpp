#include "Track.hpp"
#include "../DebugConfig.hpp"
#include <juce_dsp/juce_dsp.h>

Track::Track(juce::AudioFormatManager& fm) : formatManager(fm) {}
Track::~Track() {}

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
    // Mix all overlapping clips
    for (const auto& c : clips) {
        if (playheadSeconds < c.startTime + c.duration &&
            playheadSeconds + (double)numSamples / sampleRate > c.startTime)
        {
            auto reader = std::unique_ptr<juce::AudioFormatReader>(
                formatManager.createReaderFor(c.sourceFile));
            if (!reader) {
                DBG("Failed to create reader for: " << c.sourceFile.getFullPathName());
                continue;
            }

            double blockStartTimeSeconds = playheadSeconds;
            double blockEndTimeSeconds = playheadSeconds + (double)numSamples / sampleRate;

            double readStartTimeInClip = juce::jmax(0.0, blockStartTimeSeconds - c.startTime);
            double readEndTimeInClip = juce::jmin(c.duration, blockEndTimeSeconds - c.startTime);

            if (readStartTimeInClip >= c.duration || readEndTimeInClip <= 0.0) {
                continue;
            }

            double sourceSampleRate = reader->sampleRate;
            juce::int64 sourceFileStartSample = static_cast<juce::int64>((c.offset + readStartTimeInClip) * sourceSampleRate);
            juce::int64 sourceFileEndSample = static_cast<juce::int64>((c.offset + readEndTimeInClip) * sourceSampleRate);
            juce::int64 numSourceSamplesToRead = sourceFileEndSample - sourceFileStartSample;

            juce::int64 outputBufferStartSample = static_cast<juce::int64>(juce::jmax(0.0, (c.startTime - blockStartTimeSeconds) * sampleRate));
            if (numSourceSamplesToRead <= 0) {
                continue;
            }
            outputBufferStartSample = juce::jmax((juce::int64)0, outputBufferStartSample);

            juce::AudioBuffer<float> tempClipBuf(reader->numChannels, (int)numSourceSamplesToRead);
            tempClipBuf.clear();
            reader->read(&tempClipBuf, 0, (int)numSourceSamplesToRead, sourceFileStartSample, true, true);

            if (std::abs(sourceSampleRate - sampleRate) > 0.1) {
                double ratio = sampleRate / sourceSampleRate;
                int numOutputSamples = static_cast<int>(numSourceSamplesToRead * ratio + 0.5);
                juce::AudioBuffer<float> resampledBuffer(reader->numChannels, numOutputSamples);
                resampledBuffer.clear();
                for (int ch = 0; ch < tempClipBuf.getNumChannels(); ++ch) {
                    const float* inputData = tempClipBuf.getReadPointer(ch);
                    float* outputData = resampledBuffer.getWritePointer(ch);
                    for (int i = 0; i < numOutputSamples; ++i) {
                        double sourcePos = (double)i / ratio;
                        int baseIndex = (int)sourcePos;
                        double fraction = sourcePos - baseIndex;
                        float sample = 0.0f;
                        if (baseIndex >= 0 && baseIndex < numSourceSamplesToRead - 1) {
                            float y0 = inputData[baseIndex];
                            float y1 = inputData[baseIndex + 1];
                            double t = fraction;
                            double t2 = t * t;
                            double t3 = t2 * t;
                            double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
                            double h01 = -2.0 * t3 + 3.0 * t2;
                            sample = (float)(h00 * y0 + h01 * y1);
                        } else if (baseIndex >= 0 && baseIndex < numSourceSamplesToRead) {
                            sample = inputData[baseIndex];
                        }
                        outputData[i] = sample;
                    }
                }
                tempClipBuf = std::move(resampledBuffer);
                numSourceSamplesToRead = numOutputSamples;
            }

            juce::int64 numSamplesToWrite = juce::jmin((juce::int64)numSamples - outputBufferStartSample, numSourceSamplesToRead);
            if (numSamplesToWrite <= 0) {
                continue;
            }

            float gain = juce::Decibels::decibelsToGain(volumeDb) * c.volume;
            for (int ch = 0; ch < output.getNumChannels(); ++ch) {
                float panL = (1.0f - juce::jlimit(-1.f, 1.f, pan)) * 0.5f;
                float panR = (1.0f + juce::jlimit(-1.f, 1.f, pan)) * 0.5f;
                float panGain = (ch == 0) ? panL : panR;
                output.addFrom(ch, (int)outputBufferStartSample,
                               tempClipBuf, ch % tempClipBuf.getNumChannels(),
                               0, (int)numSamplesToWrite,
                               gain * panGain);
            }
        }
    }
}

void Track::prepareToPlay(double sampleRate, int bufferSize) {
    currentSampleRate = sampleRate;
    currentBufferSize = bufferSize;
    
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
    
    DEBUG_PRINT("Track '" << name << "' adding effect with sample rate: " << currentSampleRate << " Hz");
    
    if (!effect->loadVST(vstPath, currentSampleRate)) {
        std::cerr << "Failed to load VST: " << vstPath << std::endl;
        return nullptr;
    }
    
    effect->prepareToPlay(currentSampleRate, currentBufferSize);
    
    Effect* effectPtr = effect.get();
    effects.push_back(std::move(effect));
    
    updateEffectIndices();
    
    DEBUG_PRINT("Added effect '" << effectPtr->getName() << "' to track '" << name << "'");
    return effectPtr;
}

bool Track::removeEffect(int index) {
    if (index < 0 || index >= static_cast<int>(effects.size())) {
        DEBUG_PRINT("Invalid effect index for removal: " << index);
        return false;
    }
    
    auto& effectToRemove = effects[index];
    std::string effectName = effectToRemove->getName();
    
    DEBUG_PRINT("Removing effect '" << effectName << "' from track '" << name << "'");
    
    try {
        // First, disable the effect to stop it from being processed
        effectToRemove->disable();
        
        // Close any open editor windows
        // Note: This will be handled by the Effect destructor, but doing it here for safety
        
        // Remove from the vector - this will trigger the destructor
        effects.erase(effects.begin() + index);
        
        // Update all effect indices after removal
        updateEffectIndices();
        
        DEBUG_PRINT("Successfully removed effect '" << effectName << "' from track '" << name << "'");
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception while removing effect '" << effectName << "': " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Unknown exception while removing effect: " << effectName << std::endl;
        return false;
    }
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
            try {
                effect->processAudio(buffer);
            } catch (const std::exception& e) {
                std::cerr << "ERROR: Effect '" << effect->getName() << "' on track '" << name << "' crashed: " << e.what() << std::endl;
                effect->disable(); // Disable crashing effect
            } catch (...) {
                std::cerr << "ERROR: Effect '" << effect->getName() << "' on track '" << name << "' crashed (unknown exception)" << std::endl;
                effect->disable(); // Disable crashing effect
            }
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

void Track::clearClips() {
    clips.clear();
}