#include "AudioClip.hpp"

AudioClip::AudioClip() : startTime(0.0), offset(0.0), duration(0.0), volume(1.0f), 
                        cachedSampleRate(0.0), isLoaded(false) {}

AudioClip::AudioClip(const juce::File& sourceFile, double startTime, double offset, double duration, float volume)
    : sourceFile(sourceFile), startTime(startTime), offset(offset), duration(duration), volume(volume),
      cachedSampleRate(0.0), isLoaded(false) {}

AudioClip::AudioClip(const AudioClip& other) 
    : sourceFile(juce::File(other.sourceFile.getFullPathName())),
      startTime(other.startTime), 
      offset(other.offset), 
      duration(other.duration), 
      volume(other.volume),
      cachedSampleRate(0.0), isLoaded(false) {
}

AudioClip& AudioClip::operator=(const AudioClip& other) {
    if (this != &other) {
        sourceFile = juce::File(other.sourceFile.getFullPathName());
        startTime = other.startTime;
        offset = other.offset;
        duration = other.duration;
        volume = other.volume;
        
        unloadAudioData();
    }
    return *this;
}

void AudioClip::loadAudioData(juce::AudioFormatManager& formatManager, double targetSampleRate) const {
    if (isLoaded && cachedSampleRate == targetSampleRate && preRenderedAudio != nullptr) {
        return;
    }
    
    if (!cachedReader) {
        cachedReader = std::unique_ptr<juce::AudioFormatReader>(
            formatManager.createReaderFor(sourceFile));
        if (!cachedReader) {
            return;
        }
    }
    
    double sourceSampleRate = cachedReader->sampleRate;
    
    juce::int64 sourceStartSample = static_cast<juce::int64>(offset * sourceSampleRate);
    juce::int64 sourceEndSample = static_cast<juce::int64>((offset + duration) * sourceSampleRate);
    juce::int64 numSourceSamples = sourceEndSample - sourceStartSample;
    
    if (numSourceSamples <= 0) {
        return;
    }
    
    juce::AudioBuffer<float> sourceBuffer(cachedReader->numChannels, (int)numSourceSamples);
    sourceBuffer.clear();
    cachedReader->read(&sourceBuffer, 0, (int)numSourceSamples, sourceStartSample, true, true);
    
    if (std::abs(sourceSampleRate - targetSampleRate) > 0.1) {
        double ratio = targetSampleRate / sourceSampleRate;
        int numOutputSamples = static_cast<int>(numSourceSamples * ratio + 0.5);
        
        preRenderedAudio = std::make_unique<juce::AudioBuffer<float>>(cachedReader->numChannels, numOutputSamples);
        preRenderedAudio->clear();
        
        for (int ch = 0; ch < sourceBuffer.getNumChannels(); ++ch) {
            const float* inputData = sourceBuffer.getReadPointer(ch);
            float* outputData = preRenderedAudio->getWritePointer(ch);
            
            for (int i = 0; i < numOutputSamples; ++i) {
                double sourcePos = (double)i / ratio;
                int baseIndex = (int)sourcePos;
                double fraction = sourcePos - baseIndex;
                
                if (baseIndex >= 0 && baseIndex < numSourceSamples - 1) {
                    float y0 = inputData[baseIndex];
                    float y1 = inputData[baseIndex + 1];
                    outputData[i] = y0 + (float)fraction * (y1 - y0);
                } else if (baseIndex >= 0 && baseIndex < numSourceSamples) {
                    outputData[i] = inputData[baseIndex];
                } else {
                    outputData[i] = 0.0f;
                }
            }
        }
    } else {
        preRenderedAudio = std::make_unique<juce::AudioBuffer<float>>(sourceBuffer);
    }
    
    cachedSampleRate = targetSampleRate;
    isLoaded = true;
}

void AudioClip::unloadAudioData() const {
    cachedReader.reset();
    preRenderedAudio.reset();
    isLoaded = false;
    cachedSampleRate = 0.0;
}