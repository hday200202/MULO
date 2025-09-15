#pragma once

#include "Track.hpp"
#include "MIDIClip.hpp"

// Forward declaration
class AudioClip;

// MIDITrack - handles MIDI data and MIDI-specific functionality
class MIDITrack : public Track {
public:
    MIDITrack();
    ~MIDITrack() override = default;

    // Track type identification
    TrackType getType() const override { return TrackType::MIDI; }

    // MIDI processing implementation
    void process(double playheadSeconds, juce::AudioBuffer<float>& outputBuffer, int numSamples, double sampleRate) override;
    void prepareToPlay(double sampleRate, int bufferSize) override;

    // Audio clip management - minimal implementation for compatibility
    void clearClips() override;
    const std::vector<AudioClip>& getClips() const override;
    void addClip(const AudioClip& clip) override;
    void removeClip(size_t index) override;
    AudioClip* getReferenceClip() override;

    // MIDI clip management - the actual functionality for MIDI tracks
    void clearMIDIClips();
    const std::vector<MIDIClip>& getMIDIClips() const;
    void addMIDIClip(const MIDIClip& clip);
    void removeMIDIClip(size_t index);
    MIDIClip* getMIDIClip(size_t index);
    size_t getMIDIClipCount() const;
    
    // MIDI control methods
    void sendAllNotesOff();
    void sendMIDIMessage(const juce::MidiMessage& message);

private:
    // MIDI-specific data members
    std::vector<MIDIClip> midiClips;
    
    // Temporary empty AudioClip vector for compatibility
    std::vector<AudioClip> emptyClips;
    
    // MIDI-specific effect processing
    void processEffectsWithMidi(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer);
};
