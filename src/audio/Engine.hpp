#pragma once

#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <stack>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <atomic>
#include <nlohmann/json.hpp>

#include "Composition.hpp"
#include "../DebugConfig.hpp"

class EnginePlayHead : public juce::AudioPlayHead {
public:
    EnginePlayHead() = default;
    
    void updatePosition(double positionSeconds, double bpm, bool isPlaying, double sampleRate, int timeSigNum = 4, int timeSigDen = 4) {
        const juce::ScopedLock lock(positionLock);
        
        currentPosition = juce::AudioPlayHead::PositionInfo();
        currentPosition.setTimeInSamples(juce::int64(positionSeconds * sampleRate));
        currentPosition.setTimeInSeconds(positionSeconds);
        currentPosition.setBpm(bpm);
        
        if (isPlaying) {
            currentPosition.setIsPlaying(true);
            currentPosition.setIsRecording(false);
        } else {
            currentPosition.setIsPlaying(false);
        }
        
        currentPosition.setTimeSignature(juce::AudioPlayHead::TimeSignature{timeSigNum, timeSigDen});
        
        double secondsPerBeat = 60.0 / bpm;
        double currentBeat = positionSeconds / secondsPerBeat;
        double currentBar = currentBeat / static_cast<double>(timeSigNum);
        
        currentPosition.setBarCount(juce::int64(currentBar));
        currentPosition.setPpqPosition(currentBeat);
        currentPosition.setPpqPositionOfLastBarStart(std::floor(currentBar) * static_cast<double>(timeSigNum));
    }
    
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override {
        const juce::ScopedLock lock(positionLock);
        return currentPosition;
    }
    
private:
    juce::CriticalSection positionLock;
    juce::AudioPlayHead::PositionInfo currentPosition;
};

class Engine : public juce::AudioIODeviceCallback, public juce::MidiInputCallback {
public:
    juce::AudioFormatManager formatManager;
    
    Engine();
    ~Engine();
    
    struct PendingEffect {
        std::string trackName;
        std::string vstPath;
        bool enabled = true;
        int index = -1;
        std::vector<std::pair<int, float>> parameters;
    };
    
    struct PendingAutomation {
        std::string trackName;
        std::string effectName;
        std::string parameterName;
        std::vector<Track::AutomationPoint> points;
    };
    
    const std::vector<PendingEffect>& getPendingEffects() const { return pendingEffects; }
    void clearPendingEffects() { pendingEffects.clear(); }
    const std::vector<PendingAutomation>& getPendingAutomation() const { return pendingAutomation; }
    void clearPendingAutomation() { pendingAutomation.clear(); }

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
    void sendBpmToSynthesizers();
    
    // Track management
    void addTrack(const std::string& name = "", const std::string& samplePath = "");
    std::string addMIDITrack(const std::string& name = "");
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
    void save(const std::string& path = "untitled.mpf") const;
    std::string getStateString() const;
    void load(const std::string& state);
    
    // Audio device callbacks
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels, float* const* outputChannelData, int numOutputChannels, int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    
    // MIDI input callback
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;
    
    std::string getCurrentCompositionName() const;
    void setCurrentCompositionName(const std::string& newName);

    inline double getSampleRate() const { return sampleRate; }
    inline void setSampleRate(double newSampleRate) { 
        sampleRate = newSampleRate; 
        DEBUG_PRINT("[Engine] Sample rate set to " << newSampleRate);
    }
    
    bool configureAudioDevice(double sampleRate, int bufferSize = 256);

    std::vector<float> generateWaveformPeaks(const juce::File& audioFile, float duration, float peakResolution = 0.05f);

    void playSound(const std::string& filePath, float volume);
    void playSound(const juce::File& file, float volume);

    void sendRealtimeMIDI(int noteNumber, int velocity, bool noteOn = true);

    void setMetronomeEnabled(bool enabled);
    inline bool isMetronomeEnabled() const { return metronomeEnabled; }
    void generateMetronomeTrack();

private:
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<Composition> currentComposition;
    
    std::unique_ptr<EnginePlayHead> playHead;

    std::unique_ptr<juce::AudioFormatReaderSource> previewSource;
    juce::AudioTransportSource previewTransport;

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
    
    juce::MidiBuffer incomingMidiBuffer;
    juce::CriticalSection midiInputLock;
    
    // Thread safety for engine state changes
    juce::CriticalSection engineStateLock;
    
    int synthSilenceCountdown = 0;
    static const int SYNTH_SILENCE_CYCLES = 10;
    
    std::string vstDirectory;
    std::string sampleDirectory;
    std::vector<PendingEffect> pendingEffects;
    std::vector<PendingAutomation> pendingAutomation;
    
    // State change tracking
    mutable std::chrono::seconds lastStateChangeTimestamp;
    mutable std::string lastStateHash;
    void markStateChanged();

public:
    std::string getStateHash() const;
};