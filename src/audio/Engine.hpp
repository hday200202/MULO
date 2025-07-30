#pragma once

#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <stack>
#include <fstream>
#include <iomanip>

#include "Composition.hpp"

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
    
    // Saved Position
    void setSavedPosition(double seconds);
    double getSavedPosition() const;
    bool hasSavedPosition() const;
    
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
    void removeTrackByName(const std::string& name);
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

    // Waveform generation for UI
    std::vector<float> generateWaveformPeaks(const juce::File& audioFile, float duration, float peakResolution = 0.05f);

    void undo();
    void redo();

private:
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<Composition> currentComposition;

    bool playing = false;
    double sampleRate = 44100.0;
    double positionSeconds = 0.0;
    double savedPosition = 0.0;
    bool hasSaved = false;

    juce::AudioBuffer<float> tempMixBuffer;

    std::unique_ptr<Track> masterTrack;

    void processBlock(juce::AudioBuffer<float>& outputBuffer, int numSamples);
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
