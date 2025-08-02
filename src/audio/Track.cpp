
#include "Track.hpp"

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
}