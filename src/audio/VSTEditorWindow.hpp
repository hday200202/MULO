#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <functional>

class VSTEditorWindow : public juce::DocumentWindow
{
public:
    VSTEditorWindow(const juce::String& name, juce::AudioProcessor* processor, std::function<void()> onClose = nullptr);
    
    ~VSTEditorWindow() override;

    void closeButtonPressed() override;
    
    // Simple refresh method - only call when really needed
    void forceRefresh()
    {
        repaint();
        if (auto* editor = getContentComponent()) {
            editor->repaint();
        }
    }
    
private:
    juce::AudioProcessor* vstProcessor;
    std::function<void()> closeCallback;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VSTEditorWindow)
};