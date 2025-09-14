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

            juce::AudioBuffer<float> volPanBuf(output.getNumChannels(), (int)numSourceSamplesToRead);
            volPanBuf.clear();

            if (tempClipBuf.getNumChannels() == 1 && output.getNumChannels() == 2) {
                float leftGain = std::sqrt((1.0f - pan) / 2.0f) * juce::Decibels::decibelsToGain(volumeDb);
                float rightGain = std::sqrt((1.0f + pan) / 2.0f) * juce::Decibels::decibelsToGain(volumeDb);
                volPanBuf.copyFrom(0, 0, tempClipBuf, 0, 0, (int)numSourceSamplesToRead);
                volPanBuf.copyFrom(1, 0, tempClipBuf, 0, 0, (int)numSourceSamplesToRead);
                volPanBuf.applyGain(0, 0, (int)numSourceSamplesToRead, leftGain);
                volPanBuf.applyGain(1, 0, (int)numSourceSamplesToRead, rightGain);
            } else if (tempClipBuf.getNumChannels() == 2 && output.getNumChannels() == 2) {
                float leftGain = juce::Decibels::decibelsToGain(volumeDb) * (1.0f - juce::jmax(0.0f, pan));
                float rightGain = juce::Decibels::decibelsToGain(volumeDb) * (1.0f + juce::jmin(0.0f, pan));
                volPanBuf.copyFrom(0, 0, tempClipBuf, 0, 0, (int)numSourceSamplesToRead);
                volPanBuf.copyFrom(1, 0, tempClipBuf, 1, 0, (int)numSourceSamplesToRead);
                volPanBuf.applyGain(0, 0, (int)numSourceSamplesToRead, leftGain);
                volPanBuf.applyGain(1, 0, (int)numSourceSamplesToRead, rightGain);
            } else {
                float gain = juce::Decibels::decibelsToGain(volumeDb);
                for (int ch = 0; ch < juce::jmin(tempClipBuf.getNumChannels(), output.getNumChannels()); ++ch) {
                    volPanBuf.copyFrom(ch, 0, tempClipBuf, ch, 0, (int)numSourceSamplesToRead);
                    volPanBuf.applyGain(ch, 0, (int)numSourceSamplesToRead, gain);
                }
            }

            int outputStartSample = (int)outputBufferStartSample;
            int samplesToAdd = juce::jmin((int)numSourceSamplesToRead, numSamples - outputStartSample);
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
}
