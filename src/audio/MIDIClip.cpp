#include "MIDIClip.hpp"
#include "../DebugConfig.hpp"
#include <set>

MIDIClip::MIDIClip() 
    : startTime(0.0), offset(0.0), duration(0.0), velocity(1.0f), 
      channel(1), transpose(0) {}

MIDIClip::MIDIClip(double startTime, double duration, int channel, float velocity)
    : startTime(startTime), offset(0.0), duration(duration), velocity(velocity),
      channel(channel), transpose(0) {}

MIDIClip::MIDIClip(const juce::File& sourceFile, double startTime, double offset, double duration,
                   int channel, float velocity, int transpose)
    : sourceFile(sourceFile), startTime(startTime), offset(offset), duration(duration),
      velocity(velocity), channel(channel), transpose(transpose) {
    
    if (sourceFile.exists()) {
        loadFromFile(sourceFile);
    }
}

void MIDIClip::addNote(int noteNumber, float noteVelocity, double noteStartTime, double noteDuration) {
    if (noteStartTime < 0 || noteStartTime >= duration) return;
    
    int sampleRate = 44100; // Default sample rate for timing calculations
    int startSample = std::round(noteStartTime * sampleRate);
    int endSample = std::round((noteStartTime + noteDuration) * sampleRate);
    
    // Apply transpose
    int transposedNote = juce::jlimit(0, 127, noteNumber + transpose);
    
    // Apply velocity scaling
    int scaledVelocity = juce::jlimit(0, 127, static_cast<int>(noteVelocity * velocity * 127.0f));
    
    // Note on
    juce::MidiMessage noteOn = juce::MidiMessage::noteOn(channel, transposedNote, static_cast<juce::uint8>(scaledVelocity));
    midiData.addEvent(noteOn, startSample);
    
    // Note off
    juce::MidiMessage noteOff = juce::MidiMessage::noteOff(channel, transposedNote, static_cast<juce::uint8>(0));
    midiData.addEvent(noteOff, endSample);
    
    DEBUG_PRINT("Added MIDI note: " << transposedNote << " vel:" << scaledVelocity 
                << " start:" << noteStartTime << " dur:" << noteDuration);
}

void MIDIClip::addControlChange(int controller, int value, double time) {
    if (time < 0 || time >= duration) return;
    
    int sampleRate = 44100;
    int sample = static_cast<int>(time * sampleRate);
    
    juce::MidiMessage cc = juce::MidiMessage::controllerEvent(channel, controller, 
                                                             juce::jlimit(0, 127, value));
    midiData.addEvent(cc, sample);
    
    DEBUG_PRINT("Added MIDI CC: controller=" << controller << " value=" << value << " time=" << time);
}

void MIDIClip::addProgramChange(int program, double time) {
    if (time < 0 || time >= duration) return;
    
    int sampleRate = 44100;
    int sample = static_cast<int>(time * sampleRate);
    
    juce::MidiMessage pc = juce::MidiMessage::programChange(channel, juce::jlimit(0, 127, program));
    midiData.addEvent(pc, sample);
    
    DEBUG_PRINT("Added MIDI Program Change: program=" << program << " time=" << time);
}

void MIDIClip::clear() {
    midiData.clear();
    DEBUG_PRINT("Cleared MIDI clip data");
}

void MIDIClip::fillMidiBuffer(juce::MidiBuffer& buffer, double clipStartTime, double clipEndTime,
                             double sampleRate, int startSample) const {
    if (isEmpty() || clipEndTime <= startTime || clipStartTime > getEndTime()) {
        return; // No overlap
    }
    
    // Calculate the range within this clip
    double localStartTime = juce::jmax(0.0, clipStartTime - startTime + offset);
    double localEndTime = juce::jmin(duration, clipEndTime - startTime + offset);
    
    if (localStartTime >= localEndTime) return;
    
    int localStartSample = static_cast<int>(localStartTime * sampleRate);
    int localEndSample = static_cast<int>(localEndTime * sampleRate);
    
    std::set<int> activeNotes;
    std::set<int> allNotesPlayedInClip;
    
    double actualClipEnd = startTime + duration;
    bool isAtClipEnd = (clipEndTime >= actualClipEnd);
    
    if (isAtClipEnd) {
        for (const auto& event : midiData) {
            double eventTimeRelativeToClip = event.samplePosition / sampleRate;
            
            if (eventTimeRelativeToClip >= 0.0 && eventTimeRelativeToClip <= duration) {
                juce::MidiMessage message = event.getMessage();
                if (message.isNoteOn() && message.getVelocity() > 0) {
                    int noteNumber = message.getNoteNumber();

                    if (transpose != 0) {
                        noteNumber = juce::jlimit(0, 127, noteNumber + transpose);
                    }
                    allNotesPlayedInClip.insert(noteNumber);
                    DEBUG_PRINT("MIDIClip: Note " << noteNumber << " was played in clip");
                }
            }
        }
        DEBUG_PRINT("MIDIClip: Found " << allNotesPlayedInClip.size() << " different notes played in clip (duration=" << duration << ")");
    }
    
    // Iterate through MIDI events in the time range
    for (const auto& event : midiData) {
        int eventSample = event.samplePosition;
        
        if (eventSample >= localStartSample && eventSample < localEndSample) {
            // Calculate the output sample position
            int outputSample = startSample + (eventSample - localStartSample);
            
            // Apply velocity scaling and transpose if needed
            juce::MidiMessage message = event.getMessage();
            
            if (message.isNoteOnOrOff()) {
                int noteNumber = message.getNoteNumber();
                int channel = message.getChannel();
                
                // Apply transpose
                if (transpose != 0) {
                    noteNumber = juce::jlimit(0, 127, noteNumber + transpose);
                }
                
                if (message.isNoteOn()) {
                    activeNotes.insert(noteNumber);
                    
                    // Apply velocity scaling
                    int newVelocity = juce::jlimit(0, 127, 
                        static_cast<int>(message.getVelocity() * velocity));
                    message = juce::MidiMessage::noteOn(channel, noteNumber, 
                                                      static_cast<juce::uint8>(newVelocity));
                } else {
                    activeNotes.erase(noteNumber);
                    message = juce::MidiMessage::noteOff(channel, noteNumber);
                }
            }
            
            buffer.addEvent(message, outputSample);
        }
    }
    
    // If we're at the end of the clip, send note-offs for ALL notes that were played in this clip
    if (isAtClipEnd && !allNotesPlayedInClip.empty()) {
        int clipEndOutputSample = startSample + (localEndSample - localStartSample);
        
        DEBUG_PRINT("MIDIClip: Sending note-offs for " << allNotesPlayedInClip.size() << " notes played in clip at clip end");
        
        // Send note-off for ALL notes that were played in this clip
        for (int noteNumber : allNotesPlayedInClip) {
            juce::MidiMessage noteOff = juce::MidiMessage::noteOff(1, noteNumber);
            buffer.addEvent(noteOff, clipEndOutputSample);
            DEBUG_PRINT("MIDIClip: Note-off for note " << noteNumber << " at sample " << clipEndOutputSample);
        }
    }
}

bool MIDIClip::loadFromFile(const juce::File& file) {
    if (!file.exists() || !file.hasFileExtension("mid") && !file.hasFileExtension("midi")) {
        DEBUG_PRINT("Invalid MIDI file: " << file.getFullPathName().toStdString());
        return false;
    }
    
    juce::FileInputStream fileStream(file);
    if (!fileStream.openedOk()) {
        DEBUG_PRINT("Failed to open MIDI file: " << file.getFullPathName().toStdString());
        return false;
    }
    
    juce::MidiFile midiFile;
    if (!midiFile.readFrom(fileStream)) {
        DEBUG_PRINT("Failed to read MIDI file: " << file.getFullPathName().toStdString());
        return false;
    }
    
    // Convert to a single track with absolute timing
    midiFile.convertTimestampTicksToSeconds();
    
    midiData.clear();
    
    // Merge all tracks into our buffer
    for (int track = 0; track < midiFile.getNumTracks(); ++track) {
        const auto* midiTrack = midiFile.getTrack(track);
        
        for (int i = 0; i < midiTrack->getNumEvents(); ++i) {
            const auto* event = midiTrack->getEventPointer(i);
            if (event->message.isNoteOnOrOff() || event->message.isController() || 
                event->message.isProgramChange()) {
                
                // Convert time to samples (assuming 44.1kHz for now)
                int sampleTime = static_cast<int>(event->message.getTimeStamp() * 44100.0);
                midiData.addEvent(event->message, sampleTime);
            }
        }
    }
    
    sourceFile = file;
    DEBUG_PRINT("Loaded MIDI file: " << file.getFullPathName().toStdString() 
                << " with " << midiData.getNumEvents() << " events");
    
    return true;
}

bool MIDIClip::saveToFile(const juce::File& file) const {
    juce::MidiFile midiFile;
    juce::MidiMessageSequence sequence;
    
    // Convert our buffer to a MIDI sequence
    for (const auto& event : midiData) {
        double timeSeconds = event.samplePosition / 44100.0; // Convert back to seconds
        sequence.addEvent(juce::MidiMessage(event.getMessage()), timeSeconds);
    }
    
    midiFile.addTrack(sequence);
    midiFile.setSmpteTimeFormat(25, 40); // 25fps, 40 ticks per frame
    
    juce::FileOutputStream fileStream(file);
    if (!fileStream.openedOk()) {
        DEBUG_PRINT("Failed to create MIDI file: " << file.getFullPathName().toStdString());
        return false;
    }
    
    bool success = midiFile.writeTo(fileStream);
    DEBUG_PRINT("Saved MIDI file: " << file.getFullPathName().toStdString() << " success=" << success);
    
    return success;
}

bool MIDIClip::isEmpty() const {
    return midiData.getNumEvents() == 0;
}

bool MIDIClip::overlapsTime(double time) const {
    return time >= startTime && time < getEndTime();
}

bool MIDIClip::overlapsRange(double rangeStart, double rangeEnd) const {
    return !(rangeEnd <= startTime || rangeStart > getEndTime());
}
