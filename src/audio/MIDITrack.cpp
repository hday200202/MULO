#include "MIDITrack.hpp"
#include "AudioClip.hpp"
#include "../DebugConfig.hpp"
#include <juce_audio_basics/juce_audio_basics.h>

MIDITrack::MIDITrack() : Track() {}

void MIDITrack::process(double playheadSeconds, juce::AudioBuffer<float>& outputBuffer, int numSamples, double sampleRate) {
    outputBuffer.clear();
    
    juce::MidiBuffer midiBuffer;
    
    double bufferDuration = numSamples / sampleRate;
    double endTime = playheadSeconds + bufferDuration;
    
    std::vector<std::tuple<int, juce::MidiMessage, size_t>> allEvents;

    std::set<double> clipsStartingInBuffer;
    for (const auto& clip : midiClips) {
        double clipStartTime = clip.startTime;
        if (clipStartTime > playheadSeconds && clipStartTime <= endTime) {
            clipsStartingInBuffer.insert(clipStartTime);
        }
    }
    
    for (size_t clipIndex = 0; clipIndex < midiClips.size(); ++clipIndex) {
        auto& clip = midiClips[clipIndex];
        
        if (clip.overlapsRange(playheadSeconds, endTime)) {
            juce::MidiBuffer clipBuffer;
            
            // Check if this clip ends exactly when another clip starts
            bool hasGaplessTransition = false;
            double clipEndTime = clip.startTime + clip.duration;
            if (clipsStartingInBuffer.count(clipEndTime) > 0) {
                hasGaplessTransition = true;
            }
            
            clip.fillMidiBuffer(clipBuffer, playheadSeconds, endTime, sampleRate, 0, hasGaplessTransition);
            
            // Extract events from the clip buffer
            for (const auto& event : clipBuffer) {
                allEvents.emplace_back(event.samplePosition, event.getMessage(), clipIndex);
            }
        }
    }
    
    // Sort events by sample position
    std::sort(allEvents.begin(), allEvents.end(), 
              [](const auto& a, const auto& b) {
                  int sampleA = std::get<0>(a);
                  int sampleB = std::get<0>(b);
                  if (sampleA != sampleB) return sampleA < sampleB;
                  
                  // At the same sample time, handle note transitions carefully
                  const auto& msgA = std::get<1>(a);
                  const auto& msgB = std::get<1>(b);
                  bool aIsNoteOn = msgA.isNoteOn() && msgA.getVelocity() > 0;
                  bool bIsNoteOn = msgB.isNoteOn() && msgB.getVelocity() > 0;
                  bool aIsNoteOff = msgA.isNoteOff();
                  bool bIsNoteOff = msgB.isNoteOff();
                  
                  // For same note at same sample: note-off before note-on (seamless transition)
                  if (aIsNoteOff && bIsNoteOn && msgA.getNoteNumber() == msgB.getNoteNumber()) {
                      return true; // note-off first for same note
                  }
                  if (bIsNoteOff && aIsNoteOn && msgB.getNoteNumber() == msgA.getNoteNumber()) {
                      return false; // note-off first for same note
                  }
                  
                  // Otherwise, standard priority: note-offs before note-ons
                  if (aIsNoteOff != bIsNoteOff) return aIsNoteOff;
                  return std::get<2>(a) < std::get<2>(b); // then by clip index
              });
    
    // Add sorted events to the main buffer, handling conflicts
    std::set<int> activeNotes; 
    for (const auto& event : allEvents) {
        int samplePos = std::get<0>(event);
        juce::MidiMessage message = std::get<1>(event);
        
        if (message.isNoteOn() && message.getVelocity() > 0) {
            int note = message.getNoteNumber();
            // If note is already active, send note-off first
            if (activeNotes.count(note)) {
                midiBuffer.addEvent(juce::MidiMessage::noteOff(message.getChannel(), note), samplePos);
            }
            activeNotes.insert(note);
            midiBuffer.addEvent(message, samplePos);
        } else if (message.isNoteOff()) {
            int note = message.getNoteNumber();
            activeNotes.erase(note);
            midiBuffer.addEvent(message, samplePos);
        } else {
            midiBuffer.addEvent(message, samplePos);
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
