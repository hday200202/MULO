#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>
#include <vector>
#include <string>

class Composition;
class AudioClip;
class Instrument;
class Track;
class Effect;

/**
 * @brief Main audio engine for playback and composition management.
 * 
 * Responsibilities:
 * - Manages audio device input/output.
 * - Handles playback transport (play, pause, stop).
 * - Manages the current Composition (project/song).
 * - Processes and mixes audio from multiple tracks.
 */
class Engine : public juce::AudioIODeviceCallback {
public:
    juce::AudioFormatManager formatManager;

    Engine();
    ~Engine();

    // Playback 
    void play();
    void pause();
    void stop();
    void setPosition(double seconds);
    double getPosition() const;
    bool isPlaying() const;
    bool loadState(const std::string& filePath);


    // Composition 
    void newComposition(const std::string& name = "untitled");
    void loadComposition(const std::string& path);
    void saveComposition(const std::string& path);

    // Track Management 
    void addTrack(const std::string& name = "");
    void removeTrack(int index);
    Track* getTrack(int index);
    std::vector<Track*>& getAllTracks();

    // Audio Callbacks 
    // Project State
    // Save engine state to .mpf file (returns true on success)
    bool saveState(const std::string& path = "untitled.mpf") const;
    void audioDeviceIOCallbackWithContext(
        const float* const* inputChannelData,
        int numInputChannels,
        float* const* outputChannelData,
        int numOutputChannels,
        int numSamples,
        const juce::AudioIODeviceCallbackContext& context
    ) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    std::string getCurrentCompositionName() const;

private:
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<Composition> currentComposition;

    bool playing = false;
    double sampleRate = 44100.0;
    double positionSeconds = 0.0;

    juce::AudioBuffer<float> tempMixBuffer;

    void processBlock(juce::AudioBuffer<float>& outputBuffer, int numSamples);
};

/**
 * @brief Represents a musical composition/project.
 * 
 * Contains:
 * - Name
 * - BPM (beats per minute)
 * - Time signature
 * - Collection of tracks
 */
struct Composition {
    std::string name = "untitled";
    double bpm = 120;

    // Default 4/4 time signature
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;

    std::vector<Track*> tracks;

    Composition();
    Composition(const std::string& compositionPath);
    ~Composition();
};

/**
 * @brief Represents a single track in a composition.
 * 
 * Holds:
 * - A set of audio clips.
 * - Volume (in decibels) and pan (-1 left, 0 center, 1 right).
 * - Mixing logic for this track.
 */
class Track {
public:
    Track(juce::AudioFormatManager& formatManager);
    ~Track();

    void setName(const std::string& name);
    std::string getName() const;

    void setVolume(float db);
    float getVolume() const;

    void setPan(float pan);
    float getPan() const;

    void addClip(const AudioClip& clip);
    void removeClip(int index);
    const std::vector<AudioClip>& getClips() const;

    void process(
        double playheadSeconds,
        juce::AudioBuffer<float>& outputBuffer,
        int numSamples,
        double sampleRate
    );

private:
    std::string name;
    float volumeDb = 0.0f;
    float pan = 0.0f;

    std::vector<AudioClip> clips;

    juce::AudioFormatManager& formatManager;
};

/**
 * @brief Represents a single audio clip on a track.
 * 
 * Parameters:
 * - Source audio file (e.g., WAV, MP3)
 * - Start time in the composition
 * - Offset into the audio file
 * - Duration
 * - Volume relative to the track
 */
struct AudioClip {
    juce::File sourceFile;      // mp3, wav, flac, ... file
    double startTime;           // where it is played in the composition
    double offset;              // where the audio starts relative to the whole clip
    double duration;
    float volume;               // percentage of track volume

    AudioClip();

    AudioClip(
        const juce::File& sourceFile, 
        double startTime, 
        double offset, 
        double duration, 
        float volume = 1.f
    );
};

class Effect {
public:

private:

};

inline float floatToDecibels(float linear, float minusInfinityDb = -100.0f) {
    constexpr float reference = 0.75f; // 0.75f maps to 0 dB
    if (linear <= 0.0f)
        return minusInfinityDb;
    return 20.0f * std::log10(linear / reference);
}