#pragma once

#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <stack>
#include <fstream>
#include <iomanip>

#include "Composition.hpp"
#include "../DebugConfig.hpp"

class Engine : public juce::AudioIODeviceCallback {
public:
    juce::AudioFormatManager formatManager;
    
    Engine();
    ~Engine();
    
    // Structure for deferred effect loading
    struct PendingEffect {
        std::string trackName;
        std::string vstPath;
        bool enabled = true;
        int index = -1;
        std::vector<std::pair<int, float>> parameters;
    };
    
    // Get list of effects that should be loaded with deferred loading
    const std::vector<PendingEffect>& getPendingEffects() const { return pendingEffects; }
    void clearPendingEffects() { pendingEffects.clear(); }

    void exportMaster(const std::string& filePath);
    
    // Playback control
    void play();
    void pause();
    void stop();
    void setPosition(double seconds);
    double getPosition() const;
    bool isPlaying() const;
    
    void setSavedPosition(double seconds);
    double getSavedPosition() const;
    bool hasSavedPosition() const;
    
    // Composition management
    void newComposition(const std::string& name = "untitled");
    void loadComposition(const std::string& path);
    void saveComposition(const std::string& path);
    std::pair<int, int> getTimeSignature() const;
    double getBpm() const;
    void setBpm(double newBpm);
    
    // Track management
    void addTrack(const std::string& name = "", const std::string& samplePath = "");
    void removeTrack(int index);
    void removeTrackByName(const std::string& name);
    Track* getTrack(int index);
    Track* getTrackByName(const std::string& name);
    std::vector<std::unique_ptr<Track>>& getAllTracks();
    Track* getMasterTrack();
    
    void setSelectedTrack(const std::string& trackName);
    std::string getSelectedTrack() const;
    Track* getSelectedTrackPtr();
    bool hasSelectedTrack() const;
    
    // Directory management
    void setVSTDirectory(const std::string& directory);
    void setSampleDirectory(const std::string& directory);
    std::string getVSTDirectory() const;
    std::string getSampleDirectory() const;
    
    // File scanning utilities
    juce::File findSampleFile(const std::string& sampleName) const;
    juce::File findVSTFile(const std::string& vstName) const;
    
    // State management
    void saveState();  // Updates currentState string
    void save(const std::string& path = "untitled.mpf") const;  // Writes currentState to file
    std::string getStateString() const;
    void loadState(const std::string& state);
    
    // Audio device callbacks
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData, int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    
    std::string getCurrentCompositionName() const;
    void setCurrentCompositionName(const std::string& newName);

    inline double getSampleRate() const { return sampleRate; }
    inline void setSampleRate(double newSampleRate) { 
        sampleRate = newSampleRate; 
        DEBUG_PRINT("[Engine] Sample rate set to " << newSampleRate);
    }
    
    bool configureAudioDevice(double sampleRate, int bufferSize = 256);

    std::vector<float> generateWaveformPeaks(const juce::File& audioFile, float duration, float peakResolution = 0.05f);
    void undo();
    void redo();

    void playSound(const std::string& filePath, float volume);
    void playSound(const juce::File& file, float volume);

    // Metronome track support
    void setMetronomeEnabled(bool enabled);
    inline bool isMetronomeEnabled() const { return metronomeEnabled; }
    void generateMetronomeTrack();

private:
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<Composition> currentComposition;

    // Metronome and sample preview
    std::unique_ptr<juce::AudioFormatReaderSource> previewSource;
    juce::AudioTransportSource previewTransport;

    // Metronome track support
    std::unique_ptr<Track> metronomeTrack;
    bool metronomeEnabled = false;
    std::string metronomeDownbeatSample = "metronomeDown.wav";
    std::string metronomeUpbeatSample = "metronomeUp.wav";
    juce::File metronomeDownbeatFile;
    juce::File metronomeUpbeatFile;

    bool playing = false;
    double sampleRate = 44100.0;
    int currentBufferSize = 256;
    double positionSeconds = 0.0;
    double savedPosition = 0.0;
    bool hasSaved = false;

    juce::AudioBuffer<float> tempMixBuffer;
    std::unique_ptr<Track> masterTrack;
    std::string selectedTrackName;
    
    std::string vstDirectory;
    std::string sampleDirectory;
    
    std::string currentState;  // Current serialized engine state
    std::vector<PendingEffect> pendingEffects;  // Effects to be loaded with deferred loading

};

inline float floatToDecibels(float linear, float minusInfinityDb = -100.0f) {
    constexpr double reference = 0.75;  // 0.75 linear = 0 dB
    if (linear <= 0.0)
        return minusInfinityDb;
    return static_cast<float>(20.0 * std::log10(static_cast<double>(linear) / reference));
}

inline float decibelsToFloat(float db, float minusInfinityDb = -100.0f) {
    constexpr double reference = 0.75;  // 0.75 linear = 0 dB
    if (db <= minusInfinityDb)
        return 0.0f;
    return static_cast<float>(reference * std::pow(10.0, static_cast<double>(db) / 20.0));
}
