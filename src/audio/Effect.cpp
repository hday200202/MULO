#include "Effect.hpp"
#include "VSTPluginManager.hpp"
#include "../DebugConfig.hpp"
#include <thread>
#include <chrono>
#include <filesystem>
#include <algorithm>

// Protect against Windows min/max macros
#ifdef _WIN32
#undef max
#undef min
#endif

// VSTEditorWindow Implementation
VSTEditorWindow::VSTEditorWindow(const juce::String& name, juce::AudioProcessor* processor, std::function<void()> onClose)
    : juce::DocumentWindow(name, juce::Colours::lightgrey, juce::DocumentWindow::allButtons)
    , vstProcessor(processor)
    , closeCallback(onClose)
{
    // Platform-specific window setup
#ifdef _WIN32
    setUsingNativeTitleBar(true);
#elif defined(__APPLE__)
    setUsingNativeTitleBar(true);
#else
    // On Linux, native title bar might cause issues with some VST plugins
    setUsingNativeTitleBar(false);
#endif
    
    if (processor && processor->hasEditor()) {
        // CRITICAL: Ensure we're on the message thread
        if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
            std::cout << "ERROR: Not on message thread! VST editor will fail!" << std::endl;
            return;
        }
        
        // CRITICAL: Ensure plugin is properly prepared before creating editor
        // Note: Don't call prepareToPlay here - it should already be called by Track::addEffect
        // processor->prepareToPlay(48000.0, 512);
        
        // CRITICAL: Some VSTs require being activated before editor creation
        processor->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        
        auto* editor = processor->createEditor();
        if (editor) {
            // CRITICAL: Set content BEFORE making window visible
            setContentOwned(editor, true);
            
            // CRITICAL: Size window to editor dimensions EXACTLY
            int editorWidth = editor->getWidth();
            int editorHeight = editor->getHeight();
            
            // Ensure minimum size for usability
            editorWidth = juce::jmax(editorWidth, 300);
            editorHeight = juce::jmax(editorHeight, 200);
            
            setSize(editorWidth, editorHeight);
            setResizable(false, false);
            
            // CRITICAL: Make editor visible FIRST
            editor->setVisible(true);
            
            // NOW make window visible and bring to front
            setVisible(true);
            toFront(true);
            
            DEBUG_PRINT("VST editor window setup complete (" << editorWidth << "x" << editorHeight << ")");
            
            // Final repaint of the entire window
            repaint();
        } else {
            std::cerr << "Failed to create VST editor" << std::endl;
        }
    } else {
        std::cerr << "VST has no editor or processor is null" << std::endl;
    }
}

VSTEditorWindow::~VSTEditorWindow() {
    // Clean shutdown
}

void VSTEditorWindow::closeButtonPressed()
{
    setVisible(false);
    if (closeCallback) {
        closeCallback();
    }
}

// Effect Implementation
Effect::~Effect() {
    // Close editor window if open - smart pointer will automatically clean up
    if (editorWindow) {
        editorWindow->setVisible(false);
        editorWindow.reset();
    }
    
    // Properly release the plugin
    if (plugin) {
        plugin->suspendProcessing(true);
        plugin->releaseResources();
        plugin.reset();
    }
}

bool Effect::loadVST(const std::string& vstPath) {
    // Store the VST path for later use
    this->vstPath = vstPath;
    
    // Validate the VST file using cross-platform manager
    auto& vstManager = VSTPluginManager::getInstance();
    if (!vstManager.isValidVSTFile(vstPath)) {
        std::cerr << "Invalid or unsupported VST file: " << vstPath << std::endl;
        return false;
    }
    
    // Create plugin format manager and register VST3 format
    static juce::AudioPluginFormatManager formatManager;
    static bool formatsRegistered = false;
    
    if (!formatsRegistered) {
        formatManager.addDefaultFormats();
        formatsRegistered = true;
        DEBUG_PRINT("Registered JUCE plugin formats");
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
        DEBUG_PRINT("Trying format: " << format->getName().toStdString());
        format->findAllTypesForFile(descriptions, vstFile.getFullPathName());
        if (!descriptions.isEmpty()) {
            DEBUG_PRINT("Found " << descriptions.size() << " plugin(s) using format: " << format->getName().toStdString());
            break;
        }
    }
    
    if (descriptions.isEmpty()) {
        std::cerr << "No valid plugin found in file: " << vstPath << std::endl;
        return false;
    }
    
    // Use the first description found
    auto* description = descriptions[0];
    DEBUG_PRINT("Loading plugin: " << description->name.toStdString() 
              << " (Format: " << description->pluginFormatName.toStdString() << ")");
    
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
    
    try {
        // Safety checks before processing
        if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) {
            DEBUG_PRINT("Warning: Empty buffer passed to VST '" << name << "'");
            return;
        }
        
        // Check if plugin expects different channel configuration
        int pluginInputChannels = plugin->getTotalNumInputChannels();
        int pluginOutputChannels = plugin->getTotalNumOutputChannels();
        int bufferChannels = buffer.getNumChannels();
        
        // if (pluginInputChannels > bufferChannels) {
        //     DEBUG_PRINT("Warning: VST '" << name << "' expects " << pluginInputChannels 
        //                << " input channels but buffer has " << bufferChannels);
        // }
        
        // Create a properly sized buffer if needed
        juce::AudioBuffer<float> processBuffer;
        if (pluginOutputChannels != bufferChannels || pluginInputChannels != bufferChannels) {
            // Create buffer matching plugin requirements
            int maxChannels = (std::max)({pluginInputChannels, pluginOutputChannels, bufferChannels});
            processBuffer.setSize(maxChannels, buffer.getNumSamples());
            processBuffer.clear();
            
            // Copy input data
            for (int ch = 0; ch < (std::min)(bufferChannels, pluginInputChannels); ++ch) {
                processBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }
            
            // Process through plugin
            juce::MidiBuffer midiBuffer;
            plugin->processBlock(processBuffer, midiBuffer);
            
            // Copy output back
            for (int ch = 0; ch < (std::min)(bufferChannels, pluginOutputChannels); ++ch) {
                buffer.copyFrom(ch, 0, processBuffer, ch, 0, buffer.getNumSamples());
            }
        } else {
            // Direct processing - channels match
            juce::MidiBuffer midiBuffer;
            plugin->processBlock(buffer, midiBuffer);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: VST '" << name << "' crashed during audio processing: " << e.what() << std::endl;
        isEnabled = false; // Disable the crashing effect
    } catch (...) {
        std::cerr << "ERROR: VST '" << name << "' crashed during audio processing (unknown exception)" << std::endl;
        isEnabled = false; // Disable the crashing effect
    }
}

void Effect::openWindow() {
    if (!plugin || !hasEditorCached) {
        std::cout << "Cannot open window - plugin: " << (plugin ? "OK" : "NULL") 
                  << ", hasEditor: " << (hasEditorCached ? "YES" : "NO") << std::endl;
        return;
    }
    
    // Check if window already exists
    if (editorWindow) {
        if (!editorWindow->isVisible()) {
            editorWindow->setVisible(true);
            editorWindow->toFront(true);
        } else {
            editorWindow->toFront(true);
        }
        return;
    }
    
    // Ensure we're on the message thread for GUI operations
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        std::cout << "Not on message thread, dispatching to main thread" << std::endl;
        juce::MessageManager::callAsync([this]() {
            openWindow(); // Call recursively on main thread
        });
        return;
    }
    
    // Create new editor window with close callback to hide (not destroy)
    editorWindow = std::make_unique<VSTEditorWindow>(juce::String(getName()), plugin.get(), [this]() {
        // Just hide the window, don't destroy it
        if (editorWindow) {
            editorWindow->setVisible(false);
        }
    });
    
    std::cout << "VST editor window created successfully!" << std::endl;
}

void Effect::setParameter(int index, float value) {
    if (!plugin) return;
    
    // Fall back to deprecated method 
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4996)
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    if (index >= 0 && index < plugin->getNumParameters()) {
        plugin->setParameter(index, value);
    }
#ifdef _MSC_VER
    #pragma warning(pop)
#else
    #pragma GCC diagnostic pop
#endif
}

float Effect::getParameter(int index) const {
    if (!plugin) return 0.0f;
    
    // Fall back to deprecated method 
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4996)
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    if (index >= 0 && index < plugin->getNumParameters()) {
        return plugin->getParameter(index);
    }
#ifdef _MSC_VER
    #pragma warning(pop)
#else
    #pragma GCC diagnostic pop
#endif
    
    return 0.0f;
}

int Effect::getNumParameters() const {
    if (!plugin) return 0;
    
    // Fall back to deprecated method 
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable: 4996)
#else
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    return plugin->getNumParameters();
#ifdef _MSC_VER
    #pragma warning(pop)
#else
    #pragma GCC diagnostic pop
#endif
}