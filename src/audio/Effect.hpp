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
    void prepareToPlay(double sampleRate, int bufferSize);
    void processAudio(juce::AudioBuffer<float>& buffer);

    void openWindow();
    
    // Force refresh the VST editor window if open - call this from Application::update()
    void updateEditor() {
        if (editorWindow && editorWindow->isVisible()) {
            editorWindow->forceRefresh();
        }
    }

    void setParameter(int index, float value);
    float getParameter(int index) const;
    int getNumParameters() const;

    inline void enable() { isEnabled = true; }
    inline void disable() { isEnabled = false; }
    inline bool enabled() const { return isEnabled; }

    const std::string& getName() const { return name; }
    
    void setIndex(int idx) { index = idx; }
    int getIndex() const { return index; }

private:
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::string name;
    std::string vstPath; // Store VST path for process launching
    bool isEnabled = true;
    bool hasEditorCached = false; // Cache the editor capability at load time
    int index = -1; // Index within the track's effect list
    
    // VST Editor Window
    std::unique_ptr<VSTEditorWindow> editorWindow;
    
};