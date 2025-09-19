#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

struct MIDIClip {
    juce::File sourceFile;
    double startTime;
    double offset;
    double duration;
    float velocity;
    int channel;
    int transpose;
    
    // MIDI data storage
    juce::MidiBuffer midiData;
    
    MIDIClip();
    MIDIClip(double startTime, double duration, int channel = 1, float velocity = 1.0f);
    MIDIClip(const juce::File& sourceFile, double startTime, double offset, double duration, 
             int channel = 1, float velocity = 1.0f, int transpose = 0);
    
    void addNote(int noteNumber, float velocity, double startTime, double duration);
    void addControlChange(int controller, int value, double time);
    void addProgramChange(int program, double time);
    void clear();
    
    void fillMidiBuffer(juce::MidiBuffer& buffer, double clipStartTime, double clipEndTime, 
                       double sampleRate, int startSample) const;
    
    bool loadFromFile(const juce::File& file);
    bool saveToFile(const juce::File& file) const;
    
    bool isEmpty() const;
    double getEndTime() const { return startTime + duration; }
    bool overlapsTime(double time) const;
    bool overlapsRange(double rangeStart, double rangeEnd) const;
};
