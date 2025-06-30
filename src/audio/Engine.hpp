#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>
#include <string>

class Composition;
class AudioClip;
class Instrument;
class Track;
class Effect;

class Engine {
public:
    Engine();
    Engine(std::unique_ptr<Composition> newComposition);
    ~Engine();

    void loadComposition(const std::string& compositionPath);
    void play();
    void pause();
    void addTrack(const std::string& trackName = "");
    void removeTrack(const std::string& trackName);
    void setPosition(const double newPosition);

    std::unique_ptr<Track> getTrack(const std::string& trackName = "");
    std::vector<std::unique_ptr<Track>> getAllTracks();

private:
    std::unique_ptr<Composition> m_currentComposition;
};

struct Composition {
    std::string name = "untitled";
    double bpm = 120;

    // Default 4/4 time signature
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;

    std::vector<std::unique_ptr<Track>> tracks;

    Composition();
    Composition(const std::string& compositionPath);
    ~Composition();
};

class Track {
public:
    Track();
    ~Track();

    void setVolume(const float db);
    float getVolume() const { return m_volume; }
    
    void setPan(const float pan);
    float getPan() const { return m_pan; }

    void setTrackIndex(const int newIndex);
    int getTrackIndex() const { return m_trackIndex; }

    void toggleMute();
    bool isMuted() const { return m_mute; }

    void toggleSolo();
    bool isSolod() const { return m_solo; }

    void addClip(const AudioClip& newClip);

    const std::vector<AudioClip>& getClips() const { return m_clips; }

private:
    std::string m_name = "";

    float m_volume = 0.f;           // Always use db value
    float m_pan = 0.f;              // -1.0f for left pan, 1.0f for right pan

    int m_trackIndex = 0;           // position in arrangement

    bool m_mute = false;
    bool m_solo = false;

    std::vector<AudioClip> m_clips; // Sample clips
    std::vector<Effect> m_effects;
};

struct AudioClip {
    juce::File sourceFile;      // mp3, wav, flac, ... file
    double startTime;           // where it is played in the composition
    double offset;              // where the audio starts relative to the whole clip
    double duration;
    float volume;               // percentage of track volume

    AudioClip() : 
    startTime(0.0), 
    offset(0.0), 
    duration(0.0), 
    volume(1.0f) {}

    AudioClip(
        const juce::File& sourceFile, 
        double startTime, 
        double offset, 
        double duration, 
        float volume = 1.f
    ) : 
    sourceFile(sourceFile), 
    startTime(startTime), 
    offset(offset), 
    duration(duration), 
    volume(volume) {}
};

class Effect {
public:

private:

};