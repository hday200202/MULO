#include "Effect.hpp"
#include "VSTPluginManager.hpp"
#include "../DebugConfig.hpp"
#include <thread>
#include <chrono>
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#undef max
#undef min
#endif

VSTEditorWindow::VSTEditorWindow(const juce::String& name, juce::AudioProcessor* processor, std::function<void()> onClose)
    : juce::DocumentWindow(name, juce::Colours::lightgrey, juce::DocumentWindow::allButtons)
    , vstProcessor(processor)
    , closeCallback(onClose)
{
    setUsingNativeTitleBar(true);
    
    if (processor && processor->hasEditor()) {
        if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
            std::cout << "ERROR: Not on message thread! VST editor will fail!" << std::endl;
            return;
        }
        
        bool isDPFPlugin = processor->getName().toLowerCase().contains("dpf") ||
                           processor->getName().toLowerCase().contains("distrho");
        
        if (isDPFPlugin) {
            DEBUG_PRINT("Creating DPF plugin editor window...");
        }
        
        processor->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        
        auto* editor = processor->createEditor();
        if (editor) {
            setContentOwned(editor, true);
            
            int editorWidth = editor->getWidth();
            int editorHeight = editor->getHeight();
            
            if (isDPFPlugin && (editorWidth < 100 || editorHeight < 100)) {
                DEBUG_PRINT("DPF plugin reported small size, using defaults");
                editorWidth = 400;
                editorHeight = 300;
            }
            
            editorWidth = juce::jmax(editorWidth, 300);
            editorHeight = juce::jmax(editorHeight, 200);
            
            setSize(editorWidth, editorHeight);
            setResizable(isDPFPlugin, isDPFPlugin);
            
            editor->setVisible(true);
            setVisible(true);
            toFront(true);
            
            DEBUG_PRINT("VST editor window setup complete (" << editorWidth << "x" << editorHeight << ")");
            repaint();
        } else {
            std::cerr << "Failed to create VST editor" << std::endl;
        }
    } else {
        std::cerr << "VST has no editor or processor is null" << std::endl;
    }
}

VSTEditorWindow::~VSTEditorWindow() {
    DEBUG_PRINT("Destroying VSTEditorWindow");
    
    try {
        setContentOwned(nullptr, false);
        setVisible(false);
        DEBUG_PRINT("VSTEditorWindow destroyed successfully");
    } catch (const std::exception& e) {
        std::cerr << "Exception during VSTEditorWindow destruction: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception during VSTEditorWindow destruction" << std::endl;
    }
}

void VSTEditorWindow::closeButtonPressed()
{
    setVisible(false);
    if (closeCallback) {
        closeCallback();
    }
}

Effect::~Effect() {
    DEBUG_PRINT("Destroying Effect: " << name);
    
    try {
        if (editorWindow) {
            DEBUG_PRINT("Closing editor window for: " << name);
            editorWindow->setVisible(false);
            editorWindow.reset();
        }
        
        if (plugin) {
            DEBUG_PRINT("Releasing plugin: " << name);
            
            try {
                plugin->suspendProcessing(true);
                
                if (plugin->hasEditor()) {
                    auto* editor = plugin->getActiveEditor();
                    if (editor) {
                        DEBUG_PRINT("Plugin has active editor, will be closed during plugin destruction");
                    }
                }
                
                try {
                    plugin->releaseResources();
                    DEBUG_PRINT("Plugin resources released successfully: " << name);
                } catch (const std::exception& e) {
                    std::cerr << "Exception during plugin->releaseResources(): " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "Unknown exception during plugin->releaseResources()" << std::endl;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
            } catch (const std::exception& e) {
                std::cerr << "Exception during plugin preparation for destruction: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception during plugin preparation for destruction" << std::endl;
            }
            
            plugin.release();
            DEBUG_PRINT("Plugin ownership released (leaked) to prevent crash: " << name);
            DEBUG_PRINT("Plugin released successfully: " << name);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during Effect destruction (" << name << "): " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception during Effect destruction: " << name << std::endl;
    }
    
    DEBUG_PRINT("Effect destroyed: " << name);
}

bool Effect::loadVST(const std::string& vstPath) {
    return loadVST(vstPath, 44100.0);
}

bool Effect::loadVST(const std::string& vstPath, double sampleRate) {
    this->vstPath = vstPath;
    
    auto& vstManager = VSTPluginManager::getInstance();
    if (!vstManager.isValidVSTFile(vstPath)) {
        std::cerr << "Invalid or unsupported VST file: " << vstPath << std::endl;
        return false;
    }
    
    static juce::AudioPluginFormatManager formatManager;
    static bool formatsRegistered = false;
    
    if (!formatsRegistered) {
        formatManager.addDefaultFormats();
        formatsRegistered = true;
        DEBUG_PRINT("Registered JUCE plugin formats");
    }
    
    juce::File vstFile(vstPath);
    if (!vstFile.exists()) {
        std::cerr << "VST file does not exist: " << vstPath << std::endl;
        return false;
    }
    
    juce::OwnedArray<juce::PluginDescription> descriptions;
    
    for (auto* format : formatManager.getFormats()) {
        DEBUG_PRINT("Trying format: " << format->getName().toStdString());
        format->findAllTypesForFile(descriptions, vstFile.getFullPathName());
        if (!descriptions.isEmpty()) {
            DEBUG_PRINT("Found " << descriptions.size() << " plugin(s) using format: " << format->getName().toStdString());
            
            for (int i = 0; i < descriptions.size(); ++i) {
                auto* desc = descriptions[i];
                DEBUG_PRINT("  Plugin " << i << ": " << desc->name.toStdString() 
                          << " (Manufacturer: " << desc->manufacturerName.toStdString()
                          << ", Format: " << desc->pluginFormatName.toStdString()
                          << ", Category: " << desc->category.toStdString() << ")");
                          
                if (desc->manufacturerName.toLowerCase().contains("distrho") || 
                    desc->manufacturerName.toLowerCase().contains("dpf") ||
                    desc->category.toLowerCase().contains("dpf")) {
                    DEBUG_PRINT("  -> Detected DPF plugin");
                }
            }
            break;
        }
    }
    
    if (descriptions.isEmpty()) {
        std::cerr << "No valid plugin found in file: " << vstPath << std::endl;
        return false;
    }
    
    auto* description = descriptions[0];
    DEBUG_PRINT("Loading plugin: " << description->name.toStdString() 
              << " (Format: " << description->pluginFormatName.toStdString() << ")");
    
    bool isDPFPlugin = description->manufacturerName.toLowerCase().contains("distrho") || 
                       description->manufacturerName.toLowerCase().contains("dpf") ||
                       description->category.toLowerCase().contains("dpf");
    
    if (isDPFPlugin) {
        DEBUG_PRINT("Applying DPF-specific loading procedures...");
    }
    
    juce::String errorMessage;
    
    if (isDPFPlugin) {
        DEBUG_PRINT("Creating DPF plugin instance with sample rate: " << sampleRate << " Hz");
    }
    
    plugin = formatManager.createPluginInstance(*description, sampleRate, 512, errorMessage);
    
    if (!plugin) {
        std::cerr << "Failed to create plugin instance: " << errorMessage.toStdString() << std::endl;
        
        if (isDPFPlugin) {
            std::cerr << "DPF Plugin troubleshooting:" << std::endl;
            std::cerr << "  - Check if all DPF dependencies are installed" << std::endl;
            std::cerr << "  - Verify plugin file permissions: " << vstPath << std::endl;
            std::cerr << "  - Try running: ldd " << vstPath << std::endl;
        }
        return false;
    }
    
    name = plugin->getName().toStdString();
    
    if (isDPFPlugin) {
        DEBUG_PRINT("Initializing DPF plugin: " << name);
        
        plugin->suspendProcessing(false);
        plugin->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        
        DEBUG_PRINT("DPF plugin initialization complete");
    } else {
        plugin->suspendProcessing(false);
    }
    
    DEBUG_PRINT("VST '" << name << "' loaded with " << plugin->getParameters().size() << " parameters");
    DEBUG_PRINT("Successfully loaded VST: " << name);
    
    hasEditorCached = plugin->hasEditor();
    DEBUG_PRINT("VST '" << name << "' editor capability cached: " << (hasEditorCached ? "Yes" : "No"));
    
    return true;
}

void Effect::prepareToPlay(double sampleRate, int bufferSize) {
    if (!plugin) return;
    
    DEBUG_PRINT("Preparing VST '" << name << "' for playback...");
    
    bool isDPFPlugin = plugin->getName().toLowerCase().contains("dpf") ||
                       name.find("DPF") != std::string::npos ||
                       name.find("DISTRHO") != std::string::npos;
    
    juce::AudioProcessor::BusesLayout layout;
    
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    
    if (!plugin->setBusesLayout(layout)) {
        DEBUG_PRINT("Warning: Could not set stereo layout for VST '" << name << "'");
        
        layout.inputBuses.clear();
        layout.outputBuses.clear();
        layout.inputBuses.add(juce::AudioChannelSet::mono());
        layout.outputBuses.add(juce::AudioChannelSet::mono());
        
        if (!plugin->setBusesLayout(layout)) {
            DEBUG_PRINT("Warning: Could not set mono layout for VST '" << name << "'");
            
            // DPF plugins might have specific channel requirements
            if (isDPFPlugin) {
                DEBUG_PRINT("DPF plugin - trying default bus layout...");
                // Let DPF plugin use its preferred layout
            }
        }
    }
    
    if (isDPFPlugin) {
        DEBUG_PRINT("Applying DPF-specific preparation for '" << name << "'");
        
        plugin->suspendProcessing(false);
        plugin->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
    }
    
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
    
    if (isSynthesizer() && isSilenced()) {
        buffer.clear();
        return;
    }
    
    try {
        if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) {
            DEBUG_PRINT("Warning: Empty buffer passed to VST '" << name << "'");
            return;
        }
        
        int pluginInputChannels = plugin->getTotalNumInputChannels();
        int pluginOutputChannels = plugin->getTotalNumOutputChannels();
        int bufferChannels = buffer.getNumChannels();
        
        bool isDPFPlugin = plugin->getName().toLowerCase().contains("dpf") ||
                           name.find("DPF") != std::string::npos ||
                           name.find("DISTRHO") != std::string::npos;
        
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
            
            // DPF plugins might need MIDI even if they don't use it
            if (isDPFPlugin) {
                // Some DPF plugins expect at least an empty MIDI buffer
                midiBuffer.clear();
            }
            
            plugin->processBlock(processBuffer, midiBuffer);
            
            // Copy output back
            for (int ch = 0; ch < (std::min)(bufferChannels, pluginOutputChannels); ++ch) {
                buffer.copyFrom(ch, 0, processBuffer, ch, 0, buffer.getNumSamples());
            }
        } else {
            // Direct processing - channels match
            juce::MidiBuffer midiBuffer;
            
            // DPF plugins might need MIDI even if they don't use it
            if (isDPFPlugin) {
                midiBuffer.clear();
            }
            
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

void Effect::processAudio(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
    if (!plugin || !isEnabled) {
        DEBUG_PRINT("Effect '" << name << "' skipped - plugin: " << (plugin ? "OK" : "NULL") << ", enabled: " << (isEnabled ? "YES" : "NO"));
        return;
    }
    
    // Check if synthesizer is temporarily silenced
    if (isSynthesizer() && isSilenced()) {
        buffer.clear(); // Clear output buffer to ensure silence
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
            
            // Process through plugin with MIDI data
            plugin->processBlock(processBuffer, midiBuffer);
            
            // Copy output back
            for (int ch = 0; ch < (std::min)(bufferChannels, pluginOutputChannels); ++ch) {
                buffer.copyFrom(ch, 0, processBuffer, ch, 0, buffer.getNumSamples());
            }
        } else {
            // Direct processing - channels match, use provided MIDI buffer
            plugin->processBlock(buffer, midiBuffer);
        }
        
        // Debug output for synthesizers processing MIDI
        if (isSynthesizer() && !midiBuffer.isEmpty()) {
            DEBUG_PRINT("Synthesizer '" << name << "' processed " << midiBuffer.getNumEvents() << " MIDI events");
            
            // Log the actual MIDI events
            for (auto metadata : midiBuffer) {
                auto message = metadata.getMessage();
                if (message.isNoteOn()) {
                    DEBUG_PRINT("  MIDI Note ON: " << message.getNoteNumber() << " vel:" << message.getVelocity() << " @" << metadata.samplePosition);
                } else if (message.isNoteOff()) {
                    DEBUG_PRINT("  MIDI Note OFF: " << message.getNoteNumber() << " @" << metadata.samplePosition);
                }
            }
            
            // Check if synthesizer actually produced audio
            float maxSample = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
                for (int i = 0; i < buffer.getNumSamples(); ++i) {
                    maxSample = std::max(maxSample, std::abs(buffer.getSample(ch, i)));
                }
            }
            DEBUG_PRINT("Synthesizer '" << name << "' peak output: " << maxSample);
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
    
    // Ensure we're on the message thread for GUI operations
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        std::cout << "Not on message thread, dispatching to main thread" << std::endl;
        juce::MessageManager::callAsync([this]() {
            openWindow(); // Call recursively on main thread
        });
        return;
    }
    
    // Always destroy and recreate the window to avoid state issues
    if (editorWindow) {
        DEBUG_PRINT("Destroying existing editor window for: " << name);
        editorWindow.reset(); // Properly destroy the old window
        
        // Give JUCE time to clean up the previous window
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // Verify plugin is still valid and has an editor
    if (!plugin->hasEditor()) {
        std::cout << "Plugin no longer has editor capability: " << name << std::endl;
        return;
    }
    
    // Create new editor window with close callback to destroy (not just hide)
    try {
        DEBUG_PRINT("Creating new editor window for: " << name);
        editorWindow = std::make_unique<VSTEditorWindow>(juce::String(getName()), plugin.get(), [this]() {
            // Properly destroy the window when closed
            DEBUG_PRINT("Window closed callback for: " << name);
            if (editorWindow) {
                editorWindow.reset();
            }
        });
        
        std::cout << "VST editor window created successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception creating VST editor window: " << e.what() << std::endl;
        editorWindow.reset();
    } catch (...) {
        std::cerr << "Unknown exception creating VST editor window" << std::endl;
        editorWindow.reset();
    }
}

void Effect::setParameter(int index, float value) {
    if (!plugin) return;
    
    const auto& parameters = plugin->getParameters();
    if (index >= 0 && index < static_cast<int>(parameters.size())) {
        parameters[index]->setValue(value);
    }
}

float Effect::getParameter(int index) const {
    if (!plugin) return 0.0f;
    
    const auto& parameters = plugin->getParameters();
    if (index >= 0 && index < static_cast<int>(parameters.size())) {
        return parameters[index]->getValue();
    }
    
    return 0.0f;
}

int Effect::getNumParameters() const {
    if (!plugin) return 0;
    return plugin->getParameters().size();
}

void Effect::resetBuffers() {
    if (!plugin) return;
    
    try {
        DEBUG_PRINT("Resetting audio buffers for VST: " << name);
        
        // For synthesizers, send immediate silence commands before resetting
        if (isSynthesizer()) {
            DEBUG_PRINT("Sending immediate silence commands to synthesizer: " << name);
            
            // Temporarily silence the synthesizer to prevent any audio during reset
            setSilenced(true);
            
            // Create temporary buffers for aggressive reset
            juce::AudioBuffer<float> tempBuffer(2, 256); // Larger buffer for more aggressive reset
            tempBuffer.clear();
            juce::MidiBuffer resetMidiBuffer;
            
            // Send "All Sound Off" (MIDI CC 120) - more aggressive than "All Notes Off"
            // This immediately silences all sound, including release phases
            for (int channel = 1; channel <= 16; ++channel) {
                resetMidiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0); // All Sound Off
                resetMidiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), 0); // All Notes Off
                resetMidiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 121, 0), 0); // Reset All Controllers
                resetMidiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 123, 0), 0); // All Notes Off (alternative)
            }
            
            // Process the silence commands multiple times to ensure they take effect
            for (int i = 0; i < 3; ++i) {
                tempBuffer.clear(); // Clear buffer between processing cycles
                plugin->processBlock(tempBuffer, resetMidiBuffer);
                resetMidiBuffer.clear(); // Clear MIDI after first cycle to avoid repeated commands
            }
            
            // Now reset the plugin's internal state
            plugin->reset();
            
            // Process several cycles of silent audio to flush any remaining audio through the plugin
            juce::MidiBuffer emptyMidi;
            for (int cycle = 0; cycle < 5; ++cycle) {
                tempBuffer.clear(); // Silent input
                plugin->processBlock(tempBuffer, emptyMidi);
            }
        } else {
            // For regular effects, just reset normally
            plugin->reset();
        }
        
        DEBUG_PRINT("Buffer reset complete for VST: " << name);
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to reset buffers for VST '" << name << "': " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "ERROR: Failed to reset buffers for VST '" << name << "' (unknown exception)" << std::endl;
    }
}

void Effect::setBpm(double bpm) {
    if (!plugin) {
        DEBUG_PRINT("Cannot set BPM: No plugin loaded");
        return;
    }
    
    // Only send BPM to synthesizers (they're the ones that need tempo sync for LFOs, arp, etc.)
    if (!isSynthesizer()) {
        return;
    }
    
    try {
        DEBUG_PRINT("Setting BPM to " << bpm << " for synthesizer: " << name);
        
        // VST plugins get tempo information through the AudioPlayHead, which is set 
        // by the host (Engine) during processBlock calls. The BPM value itself
        // is passed through the AudioPlayHead's PositionInfo.
        // 
        // We don't need to send MIDI clock messages for modern VSTs - they query
        // the host's playhead for tempo information automatically.
        
        DEBUG_PRINT("BPM " << bpm << " will be provided to synthesizer '" << name << "' via AudioPlayHead");
        
    } catch (const std::exception& e) {
        DEBUG_PRINT("Exception setting BPM for VST '" << name << "': " << e.what());
    }
}

void Effect::setPlayHead(juce::AudioPlayHead* playHead) {
    if (!plugin) {
        DEBUG_PRINT("Cannot set PlayHead: No plugin loaded");
        return;
    }
    
    try {
        plugin->setPlayHead(playHead);
        DEBUG_PRINT("AudioPlayHead set for VST: " << name);
    } catch (const std::exception& e) {
        DEBUG_PRINT("Exception setting PlayHead for VST '" << name << "': " << e.what());
    }
}

bool Effect::isSynthesizer() const {
    if (!plugin) return false;
    
    // A synthesizer plugin typically:
    // 1. Accepts MIDI input
    // 2. Has no audio input or minimal audio input
    // 3. Produces audio output
    
    bool acceptsMidi = plugin->acceptsMidi();
    
    // Check input/output bus configuration
    int audioInputChannels = plugin->getTotalNumInputChannels();
    int audioOutputChannels = plugin->getTotalNumOutputChannels();
    
    // Synthesizers typically have no or minimal audio input and audio output
    bool hasAudioOutput = audioOutputChannels > 0;
    bool hasMinimalAudioInput = audioInputChannels <= 0; // No audio input for pure synths
    
    // Additional heuristics: check the plugin category if available
    juce::String category = plugin->getPluginDescription().category.toLowerCase();
    bool isInstrumentCategory = category.contains("instrument") || 
                               category.contains("synth") || 
                               category.contains("generator");
    
    // A plugin is considered a synthesizer if:
    // - It accepts MIDI input AND
    // - Has audio output AND
    // - (Has no audio input OR is categorized as instrument)
    bool isSynth = acceptsMidi && hasAudioOutput && (hasMinimalAudioInput || isInstrumentCategory);
    
    return isSynth;
}

bool Effect::isVSTSynthesizer(const std::string& vstPath) {
    auto& vstManager = VSTPluginManager::getInstance();
    if (!vstManager.isValidVSTFile(vstPath)) {
        DEBUG_PRINT("Invalid VST file for synth detection: " << vstPath);
        return false;
    }
    
    static juce::AudioPluginFormatManager formatManager;
    static bool formatsRegistered = false;
    
    if (!formatsRegistered) {
        formatManager.addDefaultFormats();
        formatsRegistered = true;
    }
    
    juce::File vstFile(vstPath);
    if (!vstFile.exists()) {
        DEBUG_PRINT("VST file does not exist for synth detection: " << vstPath);
        return false;
    }
    
    juce::OwnedArray<juce::PluginDescription> descriptions;
    
    // Find plugin descriptions
    for (auto* format : formatManager.getFormats()) {
        format->findAllTypesForFile(descriptions, vstFile.getFullPathName());
        if (!descriptions.isEmpty()) {
            break;
        }
    }
    
    if (descriptions.isEmpty()) {
        DEBUG_PRINT("No plugin found for synth detection: " << vstPath);
        return false;
    }
    
    auto* description = descriptions[0];
    
    // Create a temporary instance to check capabilities
    juce::String errorMessage;
    auto plugin = formatManager.createPluginInstance(*description, 44100.0, 512, errorMessage);
    
    if (!plugin) {
        DEBUG_PRINT("Failed to create plugin instance for synth detection: " << errorMessage.toStdString());
        return false;
    }
    
    // Check synthesizer characteristics
    bool acceptsMidi = plugin->acceptsMidi();
    int audioInputChannels = plugin->getTotalNumInputChannels();
    int audioOutputChannels = plugin->getTotalNumOutputChannels();
    bool hasAudioOutput = audioOutputChannels > 0;
    bool hasMinimalAudioInput = audioInputChannels <= 0;
    
    juce::String category = plugin->getPluginDescription().category.toLowerCase();
    bool isInstrumentCategory = category.contains("instrument") || 
                               category.contains("synth") || 
                               category.contains("generator");
    
    bool isSynth = acceptsMidi && hasAudioOutput && (hasMinimalAudioInput || isInstrumentCategory);
    
    DEBUG_PRINT("VST synth detection for '" << description->name.toStdString() << "': " << (isSynth ? "SYNTHESIZER" : "EFFECT"));
    
    return isSynth;
}