#include "MIDITrack.hpp"
#include "AudioClip.hpp"
#include "../DebugConfig.hpp"
#include <juce_audio_basics/juce_audio_basics.h>

MIDITrack::MIDITrack() : Track() {}

void MIDITrack::process(double playheadSeconds, juce::AudioBuffer<float>& outputBuffer, int numSamples, double sampleRate) {
    // Clear the audio buffer first
    outputBuffer.clear();
    
    // Process MIDI clips and generate audio if we have virtual instruments
    // TODO: Add virtual instrument support here
    // For now, we'll create a temporary MIDI buffer to process MIDI events
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
    
    // TODO: Route MIDI events to virtual instruments
    // For now, we'll just log MIDI events for debugging
    if (!midiBuffer.isEmpty()) {
        DEBUG_PRINT("MIDI Track '" << name << "' processed " << midiBuffer.getNumEvents() << " MIDI events");
    }
    
    // Apply effects (MIDI tracks can still have audio effects like reverb)
    processEffects(outputBuffer);
    
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
    DEBUG_PRINT("Cleared all MIDI clips from track '" << name << "'");
}

const std::vector<MIDIClip>& MIDITrack::getMIDIClips() const {
    return midiClips;
}

void MIDITrack::addMIDIClip(const MIDIClip& clip) {
    midiClips.push_back(clip);
    DEBUG_PRINT("Added MIDI clip to track '" << name << "' - start: " << clip.startTime 
                << " duration: " << clip.duration << " channel: " << clip.channel);
}

void MIDITrack::removeMIDIClip(size_t index) {
    if (index < midiClips.size()) {
        DEBUG_PRINT("Removed MIDI clip " << index << " from track '" << name << "'");
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
