#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <stack>
#include <fstream>
#include <iomanip>

class Composition;
class AudioClip;
class Instrument;
class Track;
class Effect;

/**
 * @brief Main audio engine for playback and composition management.
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
    
    // Composition 
    void newComposition(const std::string& name = "untitled");
    void loadComposition(const std::string& path);
    void saveComposition(const std::string& path);
    std::pair<int, int> getTimeSignature() const;
    double getBpm() const;
    void setBpm(double newBpm);
    
    // Track Management 
    void addTrack(const std::string& name = "", const std::string& samplePath = "");
    void removeTrack(int index);
    Track* getTrack(int index);
    Track* getTrackByName(const std::string& name);
    std::vector<std::unique_ptr<Track>>& getAllTracks(); // Changed to return reference to vector of unique_ptrs
    Track* getMasterTrack();
    
    // Project State
    bool saveState(const std::string& path = "untitled.mpf") const;
    std::string getStateString() const;
    void loadState(const std::string& state);
    
    // Audio Callbacks 
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
    void setCurrentCompositionName(const std::string& newName);

    inline double getSampleRate() const { return sampleRate; }
    inline void setSampleRate(double newSampleRate)
    { sampleRate = newSampleRate; std::cout << "[Engine] Sample rate set to " << newSampleRate << std::endl;}

    void undo();
    void redo();

private:
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<Composition> currentComposition;

    bool playing = false;
    double sampleRate = 44100.0;
    double positionSeconds = 0.0;

    juce::AudioBuffer<float> tempMixBuffer;

    std::unique_ptr<Track> masterTrack;

    void processBlock(juce::AudioBuffer<float>& outputBuffer, int numSamples);
};

/**
 * @brief Represents a musical composition/project.
 */
struct Composition {
    std::string name = "untitled";
    double bpm = 120;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;

    std::vector<std::unique_ptr<Track>> tracks; // Changed to unique_ptr

    Composition();
    Composition(const std::string& compositionPath);
    ~Composition();
};

/**
 * @brief Represents a single track in a composition.
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

    void setReferenceClip(const AudioClip& clip);
    AudioClip* getReferenceClip();

    void process(
        double playheadSeconds,
        juce::AudioBuffer<float>& outputBuffer,
        int numSamples,
        double sampleRate
    );

    void toggleMute() { muted = !muted; }
    bool isMuted() const { return muted; }

    void setSolo(bool solo) { soloed = solo; }
    bool isSolo() const { return soloed; }

private:
    std::string name;
    float volumeDb = 0.0f;
    float pan = 0.0f;
    bool muted = false;
    bool soloed = false;

    std::vector<AudioClip> clips;
    std::unique_ptr<AudioClip> referenceClip; // Changed to unique_ptr

    juce::AudioFormatManager& formatManager;
};

/**
 * @brief Represents a single audio clip on a track.
 */
struct AudioClip {
    juce::File sourceFile;
    double startTime;
    double offset;
    double duration;
    float volume;

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
    constexpr float reference = 0.75f;
    if (linear <= 0.0f)
        return minusInfinityDb;
    return 20.f * std::log10(linear / reference);
}

inline float decibelsToFloat(float db, float minusInfinityDb = -100.0f) {
    constexpr float reference = 0.75f;
    if (db <= minusInfinityDb)
        return 0.0f;
    return reference * std::pow(10.0f, db / 20.f);
}
