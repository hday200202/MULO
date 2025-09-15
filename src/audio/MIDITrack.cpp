#include "MIDITrack.hpp"
#include "AudioClip.hpp"
#include "../DebugConfig.hpp"
#include <juce_audio_basics/juce_audio_basics.h>

MIDITrack::MIDITrack() : Track() {}

void MIDITrack::process(double playheadSeconds, juce::AudioBuffer<float>& outputBuffer, int numSamples, double sampleRate) {
    // Clear the audio buffer first
    outputBuffer.clear();
    
    // Create MIDI buffer to collect all MIDI events for this time range
    juce::MidiBuffer midiBuffer;
    
    // Calculate time range for this buffer
    double bufferDuration = numSamples / sampleRate;
    double endTime = playheadSeconds + bufferDuration;
    
    // Process all MIDI clips that overlap with this time range
    for (const auto& clip : midiClips) {
        if (clip.overlapsRange(playheadSeconds, endTime)) {
            clip.fillMidiBuffer(midiBuffer, playheadSeconds, endTime, sampleRate, 0);
        }
    }
    
    // Process effects, passing MIDI data to synthesizers
    processEffectsWithMidi(outputBuffer, midiBuffer);
    
    // Apply track volume and mute
    if (muted) {
        outputBuffer.clear();
    } else {
        float trackGain = juce::Decibels::decibelsToGain(volumeDb);
        outputBuffer.applyGain(trackGain);
    }
}

void MIDITrack::prepareToPlay(double sampleRate, int bufferSize) {
    currentSampleRate = sampleRate;
    currentBufferSize = bufferSize;
    
    // Prepare all effects
    for (auto& effect : effects) {
        if (effect) {
            effect->prepareToPlay(sampleRate, bufferSize);
        }
    }
}

// Audio clip management - minimal implementation for compatibility
void MIDITrack::clearClips() {
    // MIDI tracks don't use audio clips, but this is required for interface compatibility
}

const std::vector<AudioClip>& MIDITrack::getClips() const {
    // Return empty vector - MIDI tracks don't use audio clips
    return emptyClips;
}

void MIDITrack::addClip(const AudioClip& clip) {
    // MIDI tracks don't use audio clips - this is a no-op for interface compatibility
}

void MIDITrack::removeClip(size_t index) {
    // MIDI tracks don't use audio clips - this is a no-op for interface compatibility  
}

AudioClip* MIDITrack::getReferenceClip() {
    // MIDI tracks don't use audio clips
    return nullptr;
}

// MIDI clip management - the actual functionality for MIDI tracks
void MIDITrack::clearMIDIClips() {
    midiClips.clear();
}

const std::vector<MIDIClip>& MIDITrack::getMIDIClips() const {
    return midiClips;
}

void MIDITrack::addMIDIClip(const MIDIClip& clip) {
    midiClips.push_back(clip);
}

void MIDITrack::removeMIDIClip(size_t index) {
    if (index < midiClips.size()) {
        midiClips.erase(midiClips.begin() + index);
    }
}

MIDIClip* MIDITrack::getMIDIClip(size_t index) {
    if (index < midiClips.size()) {
        return &midiClips[index];
    }
    return nullptr;
}

size_t MIDITrack::getMIDIClipCount() const {
    return midiClips.size();
}

void MIDITrack::processEffectsWithMidi(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
    for (const auto& effect : effects) {
        if (effect && effect->enabled()) {
            if (effect->isSynthesizer()) {
                // Synthesizers need both audio buffer and MIDI data
                effect->processAudio(buffer, midiBuffer);
            } else {
                // Regular effects only process audio
                effect->processAudio(buffer);
            }
        }
    }
}

void MIDITrack::sendAllNotesOff() {
    // Send "All Notes Off" (MIDI CC 123) to all synthesizers
    juce::MidiMessage allNotesOff = juce::MidiMessage::allNotesOff(1); // Channel 1
    sendMIDIMessage(allNotesOff);
    
    // Also send on other common MIDI channels
    for (int channel = 1; channel <= 16; ++channel) {
        juce::MidiMessage msg = juce::MidiMessage::allNotesOff(channel);
        sendMIDIMessage(msg);
    }
}

void MIDITrack::sendMIDIMessage(const juce::MidiMessage& message) {
    // Create a temporary MIDI buffer with the message
    juce::MidiBuffer midiBuffer;
    midiBuffer.addEvent(message, 0);
    
    // Use standard buffer size for occasional direct MIDI sends (like All Notes Off)
    int bufferSize = std::max(256, currentBufferSize);
    juce::AudioBuffer<float> tempBuffer(2, bufferSize);
    tempBuffer.clear();
    
    // Send the MIDI message to all synthesizer effects
    for (const auto& effect : effects) {
        if (effect && effect->enabled() && effect->isSynthesizer()) {
            effect->processAudio(tempBuffer, midiBuffer);
        }
    }
}
