#pragma once

#include <memory>
#include <string>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_events/juce_events.h>

// Forward declarations
class Effect;

// Custom component to hold VST editor properly on Linux
class VSTEditorComponent : public juce::Component {
public:
    VSTEditorComponent(std::unique_ptr<juce::AudioProcessorEditor> editor);
    ~VSTEditorComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentHierarchyChanged() override;
    
private:
    std::unique_ptr<juce::AudioProcessorEditor> vstEditor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTEditorComponent)
};

// Custom DocumentWindow with proper close handling and X11 cleanup
class VSTPluginWindow : public juce::DocumentWindow {
public:
    VSTPluginWindow(const juce::String& name, Effect* parentEffect);
    ~VSTPluginWindow() override;
    
    void closeButtonPressed() override;
    void userTriedToCloseWindow() override;
    
private:
    Effect* effect;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTPluginWindow)
};

class Effect {
public:
    Effect() = default;
    ~Effect();
    
    bool loadVST(const std::string& vstPath);
    void prepareToPlay(double sampleRate, int bufferSize);
    void processAudio(juce::AudioBuffer<float>& buffer);

    void openWindow();
    void closeWindow();
    bool hasWindow() const;

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
    std::unique_ptr<VSTPluginWindow> pluginWindow;
    std::string name;
    bool isEnabled = true;
    bool hasEditorCached = false; // Cache the editor capability at load time
    int index = -1; // Index within the track's effect list
    
};