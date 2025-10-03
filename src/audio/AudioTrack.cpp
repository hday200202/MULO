#include "AudioTrack.hpp"
#include "../DebugConfig.hpp"
#include <juce_dsp/juce_dsp.h>

AudioTrack::AudioTrack(juce::AudioFormatManager& fm) : Track(), formatManager(fm) {}

void AudioTrack::addClip(const AudioClip& c) { clips.push_back(c); }

void AudioTrack::removeClip(size_t idx) {
    if (idx < clips.size()) {
        clips.erase(clips.begin() + idx);
    }
}

const std::vector<AudioClip>& AudioTrack::getClips() const { return clips; }

void AudioTrack::clearClips() { clips.clear(); }

void AudioTrack::setReferenceClip(const AudioClip& clip) {
    referenceClip = std::make_unique<AudioClip>(clip);
}

AudioClip* AudioTrack::getReferenceClip() {
    return referenceClip.get();
}

void AudioTrack::process(double playheadSeconds,
                    juce::AudioBuffer<float>& output,
                    int numSamples,
                    double sampleRate) {
    if (muted) {
        return;
    }
    
    double blockEndTime = playheadSeconds + (double)numSamples / sampleRate;
    bool hasActiveClips = false;
    for (const auto& c : clips) {
        if (playheadSeconds < c.startTime + c.duration && blockEndTime > c.startTime) {
            hasActiveClips = true;
            break;
        }
    }
    
    if (!hasActiveClips) {
        return;
    }
    
    for (const auto& c : clips) {
        if (playheadSeconds < c.startTime + c.duration &&
            playheadSeconds + (double)numSamples / sampleRate > c.startTime)
        {
            c.loadAudioData(formatManager, sampleRate);
            
            if (!c.isAudioDataLoaded()) {
                DBG("Failed to load audio data for: " << c.sourceFile.getFullPathName());
                continue;
            }

            double blockStartTimeSeconds = playheadSeconds;
            double blockEndTimeSeconds = playheadSeconds + (double)numSamples / sampleRate;

            double readStartTimeInClip = juce::jmax(0.0, blockStartTimeSeconds - c.startTime);
            double readEndTimeInClip = juce::jmin(c.duration, blockEndTimeSeconds - c.startTime);

            if (readStartTimeInClip >= c.duration || readEndTimeInClip <= 0.0) {
                continue;
            }

            int startSampleInClip = static_cast<int>(readStartTimeInClip * sampleRate);
            int endSampleInClip = static_cast<int>(readEndTimeInClip * sampleRate);
            int numSamplesToRead = endSampleInClip - startSampleInClip;

            if (numSamplesToRead <= 0 || startSampleInClip >= c.preRenderedAudio->getNumSamples()) {
                continue;
            }
            
            numSamplesToRead = juce::jmin(numSamplesToRead, c.preRenderedAudio->getNumSamples() - startSampleInClip);

            juce::int64 outputBufferStartSample = static_cast<juce::int64>(juce::jmax(0.0, (c.startTime - blockStartTimeSeconds) * sampleRate));
            outputBufferStartSample = juce::jmax((juce::int64)0, outputBufferStartSample);

            juce::AudioBuffer<float> volPanBuf(output.getNumChannels(), numSamplesToRead);
            volPanBuf.clear();

            if (c.preRenderedAudio->getNumChannels() == 1 && output.getNumChannels() == 2) {
                float leftGain = std::sqrt((1.0f - pan) / 2.0f) * juce::Decibels::decibelsToGain(volumeDb);
                float rightGain = std::sqrt((1.0f + pan) / 2.0f) * juce::Decibels::decibelsToGain(volumeDb);
                
                volPanBuf.copyFrom(0, 0, *c.preRenderedAudio, 0, startSampleInClip, numSamplesToRead);
                volPanBuf.copyFrom(1, 0, *c.preRenderedAudio, 0, startSampleInClip, numSamplesToRead);
                volPanBuf.applyGain(0, 0, numSamplesToRead, leftGain);
                volPanBuf.applyGain(1, 0, numSamplesToRead, rightGain);
            } else if (c.preRenderedAudio->getNumChannels() == 2 && output.getNumChannels() == 2) {
                float leftGain = juce::Decibels::decibelsToGain(volumeDb) * (1.0f - juce::jmax(0.0f, pan));
                float rightGain = juce::Decibels::decibelsToGain(volumeDb) * (1.0f + juce::jmin(0.0f, pan));
                
                volPanBuf.copyFrom(0, 0, *c.preRenderedAudio, 0, startSampleInClip, numSamplesToRead);
                volPanBuf.copyFrom(1, 0, *c.preRenderedAudio, 1, startSampleInClip, numSamplesToRead);
                volPanBuf.applyGain(0, 0, numSamplesToRead, leftGain);
                volPanBuf.applyGain(1, 0, numSamplesToRead, rightGain);
            } else {
                float gain = juce::Decibels::decibelsToGain(volumeDb);
                for (int ch = 0; ch < juce::jmin(c.preRenderedAudio->getNumChannels(), output.getNumChannels()); ++ch) {
                    volPanBuf.copyFrom(ch, 0, *c.preRenderedAudio, ch, startSampleInClip, numSamplesToRead);
                    volPanBuf.applyGain(ch, 0, numSamplesToRead, gain);
                }
            }

            // Mix into output buffer
            int outputStartSample = (int)outputBufferStartSample;
            int samplesToAdd = juce::jmin(numSamplesToRead, numSamples - outputStartSample);
            if (outputStartSample >= 0 && samplesToAdd > 0 && outputStartSample + samplesToAdd <= numSamples) {
                for (int ch = 0; ch < juce::jmin(volPanBuf.getNumChannels(), output.getNumChannels()); ++ch) {
                    output.addFrom(ch, outputStartSample, volPanBuf, ch, 0, samplesToAdd);
                }
            }
        }
    }

    // Apply effects
    processEffects(output);

    // Apply track volume and mute
    if (muted) {
        output.clear();
    } else {
        float trackGain = juce::Decibels::decibelsToGain(volumeDb);
        output.applyGain(trackGain);
    }
}

void AudioTrack::prepareToPlay(double sampleRate, int bufferSize) {
    currentSampleRate = sampleRate;
    currentBufferSize = bufferSize;
    
    // Prepare all effects
    for (auto& effect : effects) {
        if (effect) {
            effect->prepareToPlay(sampleRate, bufferSize);
        }
    }
    
    preloadAllClips(sampleRate);
}

void AudioTrack::preloadAllClips(double sampleRate) {
    for (const auto& clip : clips) {
        clip.loadAudioData(formatManager, sampleRate);
    }
    
    if (referenceClip) {
        referenceClip->loadAudioData(formatManager, sampleRate);
    }
}

void AudioTrack::unloadAllClips() {
    for (const auto& clip : clips) {
        clip.unloadAudioData();
    }
    
    if (referenceClip) {
        referenceClip->unloadAudioData();
    }
}
