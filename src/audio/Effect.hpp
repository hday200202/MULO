#pragma once

#include <memory>
#include <string>
#include <juce_audio_processors/juce_audio_processors.h>
#include "VSTEditorWindow.hpp"

class Effect {
public:
    Effect() = default;
    ~Effect();
    
    bool loadVST(const std::string& vstPath);
    bool loadVST(const std::string& vstPath, double sampleRate);
    void prepareToPlay(double sampleRate, int bufferSize);
    void processAudio(juce::AudioBuffer<float>& buffer);
    void processAudio(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer);
    void openWindow();
    void closeWindow() {
        if (editorWindow) {
            editorWindow.reset(); // Properly destroy the window
        }
    }
    bool hasEditor() const { return hasEditorCached; }
    
    void updateEditor() {
        if (editorWindow && editorWindow->isVisible()) {
            editorWindow->forceRefresh();
        }
    }

    void setParameter(int index, float value);
    float getParameter(int index) const;
    int getNumParameters() const;
    
    void resetBuffers();  // Clear internal audio buffers (reverb, delay, etc.)
    void setBpm(double bpm);  // Send BPM to synthesizer for tempo sync
    void setPlayHead(juce::AudioPlayHead* playHead);  // Set AudioPlayHead for tempo sync
    
    // Silencing methods for synthesizer control during engine transitions
    void setSilenced(bool silenced) { silencedFlag = silenced; }
    bool isSilenced() const { return silencedFlag; }

    inline void enable() { isEnabled = true; }
    inline void disable() { isEnabled = false; }
    inline bool enabled() const { return isEnabled; }

    const std::string& getName() const { return name; }
    const std::string& getVSTPath() const { return vstPath; }
    
    void setIndex(int idx) { index = idx; }
    int getIndex() const { return index; }
    
    bool isSynthesizer() const;
    static bool isVSTSynthesizer(const std::string& vstPath);

private:
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::string name;
    std::string vstPath;
    bool isEnabled = true;
    bool hasEditorCached = false;
    bool silencedFlag = false;  // Temporary silencing for synthesizers during transitions
    int index = -1;
    
    std::unique_ptr<VSTEditorWindow> editorWindow;
};