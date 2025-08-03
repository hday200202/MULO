#include "Effect.hpp"
#include "../DebugConfig.hpp"

// VSTEditorComponent Implementation
VSTEditorComponent::VSTEditorComponent(std::unique_ptr<juce::AudioProcessorEditor> editor)
    : vstEditor(std::move(editor)) {
    if (vstEditor) {
        addAndMakeVisible(vstEditor.get());
        int width = vstEditor->getWidth();
        int height = vstEditor->getHeight();
        setSize(width, height);
        vstEditor->setBounds(0, 0, width, height);
        DEBUG_PRINT("VSTEditorComponent created with size: " << width << "x" << height);
    }
}

VSTEditorComponent::~VSTEditorComponent() {
    if (vstEditor) {
        removeChildComponent(vstEditor.get());
    }
}

void VSTEditorComponent::paint(juce::Graphics& g) {
    g.fillAll(juce::Colours::black);
}

void VSTEditorComponent::resized() {
    if (vstEditor) {
        // Make the VST editor fill the entire component
        vstEditor->setBounds(getLocalBounds());
        DEBUG_PRINT("VSTEditorComponent resized to: " << getWidth() << "x" << getHeight());
    }
}

void VSTEditorComponent::parentHierarchyChanged() {
    if (vstEditor) {
        vstEditor->repaint();
    }
}

// VSTPluginWindow Implementation
VSTPluginWindow::VSTPluginWindow(const juce::String& name, Effect* parentEffect)
    : juce::DocumentWindow(name, juce::Colours::darkgrey, juce::DocumentWindow::closeButton), 
      effect(parentEffect) {
    setUsingNativeTitleBar(true);
    setDropShadowEnabled(false);
    setResizable(false, false);
}

VSTPluginWindow::~VSTPluginWindow() {
    // Proper cleanup to avoid X11 errors
    setContentOwned(nullptr, false);
    clearContentComponent();
}

void VSTPluginWindow::closeButtonPressed() {
    setVisible(false);
    if (effect) {
        effect->closeWindow();
    }
}

void VSTPluginWindow::userTriedToCloseWindow() {
    closeButtonPressed();
}

// Effect Implementation
Effect::~Effect() {
    // Properly destroy the window if it exists
    if (pluginWindow) {
        pluginWindow->setVisible(false);
        pluginWindow->setContentOwned(nullptr, false);
        pluginWindow->clearContentComponent();
        pluginWindow->removeFromDesktop();
        pluginWindow.reset();
    }
    
    // Properly release the plugin
    if (plugin) {
        plugin->suspendProcessing(true);
        plugin->releaseResources();
        plugin.reset();
    }
}

bool Effect::loadVST(const std::string& vstPath) {
    // Create plugin format manager and register VST3 format
    static juce::AudioPluginFormatManager formatManager;
    static bool formatsRegistered = false;
    
    if (!formatsRegistered) {
        formatManager.addDefaultFormats();
        formatsRegistered = true;
    }
    
    // Create plugin description from file
    juce::File vstFile(vstPath);
    if (!vstFile.exists()) {
        std::cerr << "VST file does not exist: " << vstPath << std::endl;
        return false;
    }
    
    // Get available formats and try to load the plugin
    juce::OwnedArray<juce::PluginDescription> descriptions;
    
    for (auto* format : formatManager.getFormats()) {
        format->findAllTypesForFile(descriptions, vstFile.getFullPathName());
    }
    
    if (descriptions.isEmpty()) {
        std::cerr << "No valid plugin found in file: " << vstPath << std::endl;
        return false;
    }
    
    // Use the first description found
    auto* description = descriptions[0];
    
    // Create the plugin instance - use device sample rate and buffer size (will be set in prepareToPlay)
    juce::String errorMessage;
    plugin = formatManager.createPluginInstance(*description, 44100.0, 512, errorMessage);
    
    if (!plugin) {
        std::cerr << "Failed to create plugin instance: " << errorMessage.toStdString() << std::endl;
        return false;
    }
    
    // Set the name from the plugin
    name = plugin->getName().toStdString();
    
    // Make sure the plugin is not bypassed
    plugin->suspendProcessing(false);
    
    // Print parameter info for debugging
    DEBUG_PRINT("VST '" << name << "' has " << plugin->getNumParameters() << " parameters:");
    for (int i = 0; i < juce::jmin(10, plugin->getNumParameters()); ++i) {
        DEBUG_PRINT("  [" << i << "] " << plugin->getParameterName(i).toStdString() 
                  << " = " << plugin->getParameter(i));
    }
    
    DEBUG_PRINT("Successfully loaded VST: " << name);
    
    // Cache the editor capability immediately after successful load
    // This prevents issues where hasEditor() might return different values later
    hasEditorCached = plugin->hasEditor();
    DEBUG_PRINT("VST '" << name << "' editor capability cached: " << (hasEditorCached ? "Yes" : "No"));
    
    return true;
}

void Effect::prepareToPlay(double sampleRate, int bufferSize) {
    if (!plugin) return;
    
    // Set up proper channel layout before preparing
    juce::AudioProcessor::BusesLayout layout;
    
    // Most effects expect stereo input/output
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    
    // Try to set the layout
    if (!plugin->setBusesLayout(layout)) {
        DEBUG_PRINT("Warning: Could not set stereo layout for VST '" << name << "'");
        
        // Try with mono if stereo fails
        layout.inputBuses.clear();
        layout.outputBuses.clear();
        layout.inputBuses.add(juce::AudioChannelSet::mono());
        layout.outputBuses.add(juce::AudioChannelSet::mono());
        
        if (!plugin->setBusesLayout(layout)) {
            DEBUG_PRINT("Warning: Could not set mono layout for VST '" << name << "'");
        }
    }
    
    // Prepare the plugin with the correct audio settings
    plugin->prepareToPlay(sampleRate, bufferSize);
    
    DEBUG_PRINT("Prepared VST '" << name << "' for playback: " 
              << sampleRate << " Hz, " << bufferSize << " samples, "
              << plugin->getTotalNumInputChannels() << " in, "
              << plugin->getTotalNumOutputChannels() << " out");
}

void Effect::processAudio(juce::AudioBuffer<float>& buffer) {
    if (!plugin || !isEnabled) {
        return;
    }
    
    // Simple direct processing - no channel conversion for now
    juce::MidiBuffer midiBuffer;
    plugin->processBlock(buffer, midiBuffer);
}

void Effect::openWindow() {
    if (!plugin) {
        return;
    }
    
    // Use the cached value as the authoritative source
    if (!hasEditorCached) {
        return;
    }
    
    if (pluginWindow) {
        // Simply show the existing window
        pluginWindow->setVisible(true);
        pluginWindow->toFront(true);
        return;
    }
    
    // Ensure we're on the message thread for GUI operations
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        juce::MessageManager::callAsync([this]() {
            openWindow();
        });
        return;
    }
    
    try {
        // Create the VST editor
        auto vstEditor = plugin->createEditor();
        if (!vstEditor) {
            return;
        }
        
        // Get the proper size from the VST editor
        int editorWidth = vstEditor->getWidth();
        int editorHeight = vstEditor->getHeight();
        
        // Some VST plugins don't report proper initial size, so we need to handle this
        if (editorWidth <= 0 || editorHeight <= 0) {
            editorWidth = 400;  // Fallback width
            editorHeight = 300; // Fallback height
            vstEditor->setSize(editorWidth, editorHeight);
        }
        
        // Wrap editor in our custom component - createEditor returns a raw pointer so we need to wrap it
        std::unique_ptr<juce::AudioProcessorEditor> editorPtr(vstEditor);
        auto editorComponent = std::make_unique<VSTEditorComponent>(std::move(editorPtr));
        
        // Create our custom DocumentWindow for better Linux compatibility and close handling
        pluginWindow = std::make_unique<VSTPluginWindow>(juce::String(getName()), this);
        
        // Configure the window and set content
        pluginWindow->setContentOwned(editorComponent.release(), true);
        
        // Set the window to match the VST editor size exactly
        pluginWindow->setSize(editorWidth, editorHeight);
        
        // Center the window first
        pluginWindow->centreWithSize(editorWidth, editorHeight);
        
        // Show the window
        pluginWindow->setVisible(true);
        pluginWindow->toFront(true);
        
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while opening VST window for '" << getName() << "': " << e.what() << std::endl;
        if (pluginWindow) {
            pluginWindow.reset();
        }
    }
    catch (...) {
        std::cerr << "Unknown exception while opening VST window for '" << getName() << "'" << std::endl;
        if (pluginWindow) {
            pluginWindow.reset();
        }
    }
}

void Effect::closeWindow() {
    if (!pluginWindow) {
        return; // No window to close
    }
    
    try {
        // Instead of destroying the window completely, just hide it
        // This prevents VST3 plugin corruption issues
        pluginWindow->setVisible(false);
        
        // Note: We don't reset the window pointer - we keep it for reuse
        // This prevents the VST3 plugin editor corruption that causes crashes
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while hiding VST window for '" << getName() << "': " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown exception while hiding VST window for '" << getName() << "'" << std::endl;
    }
}

bool Effect::hasWindow() const {
    return plugin && hasEditorCached;
}

void Effect::setParameter(int index, float value) {
    if (!plugin) return;
    
    // Fall back to deprecated method 
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    if (index >= 0 && index < plugin->getNumParameters()) {
        plugin->setParameter(index, value);
    }
    #pragma GCC diagnostic pop
}

float Effect::getParameter(int index) const {
    if (!plugin) return 0.0f;
    
    // Fall back to deprecated method 
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    if (index >= 0 && index < plugin->getNumParameters()) {
        return plugin->getParameter(index);
    }
    #pragma GCC diagnostic pop
    
    return 0.0f;
}

int Effect::getNumParameters() const {
    if (!plugin) return 0;
    
    // Fall back to deprecated method 
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return plugin->getNumParameters();
    #pragma GCC diagnostic pop
}