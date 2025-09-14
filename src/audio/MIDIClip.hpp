#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

struct MIDIClip {
    juce::File sourceFile;          // MIDI file source (optional)
    double startTime;               // When this clip starts in the timeline (seconds)
    double offset;                  // Offset into the MIDI data (seconds)
    double duration;                // Duration of the clip (seconds)
    float velocity;                 // Overall velocity multiplier (0.0 - 1.0)
    int channel;                    // MIDI channel (1-16, 0 = all channels)
    int transpose;                  // Semitone transpose (-24 to +24)
    
    // MIDI data storage
    juce::MidiBuffer midiData;      // The actual MIDI events
    
    // Constructors
    MIDIClip();
    MIDIClip(double startTime, double duration, int channel = 1, float velocity = 1.0f);
    MIDIClip(const juce::File& sourceFile, double startTime, double offset, double duration, 
             int channel = 1, float velocity = 1.0f, int transpose = 0);
    
    // MIDI manipulation methods
    void addNote(int noteNumber, float velocity, double startTime, double duration);
    void addControlChange(int controller, int value, double time);
    void addProgramChange(int program, double time);
    void clear();
    
    // Get MIDI events for a specific time range
    void fillMidiBuffer(juce::MidiBuffer& buffer, double clipStartTime, double clipEndTime, 
                       double sampleRate, int startSample) const;
    
    // Load/save MIDI data
    bool loadFromFile(const juce::File& file);
    bool saveToFile(const juce::File& file) const;
    
    // Utility methods
    bool isEmpty() const;
    double getEndTime() const { return startTime + duration; }
    bool overlapsTime(double time) const;
    bool overlapsRange(double rangeStart, double rangeEnd) const;
};
