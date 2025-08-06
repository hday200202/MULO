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
            
            std::cerr << "WARNING: Skipping plugin destruction for '" << name 
                      << "' to prevent 'pure virtual method called' crash." << std::endl;
            std::cerr << "This is a known workaround for buggy VST plugins. Memory will be leaked." << std::endl;
            
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
        
        // Some DPF plugins need to be resumed before they work properly
        plugin->suspendProcessing(false);
        
        // Ensure proper state (no blocking sleep - let initialization happen naturally)
        plugin->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        
        DEBUG_PRINT("DPF plugin initialization complete");
    } else {
        // Make sure the plugin is not bypassed
        plugin->suspendProcessing(false);
    }
    
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
    
    DEBUG_PRINT("Preparing VST '" << name << "' for playback...");
    
    // Check if this is a DPF plugin
    bool isDPFPlugin = plugin->getName().toLowerCase().contains("dpf") ||
                       name.find("DPF") != std::string::npos ||
                       name.find("DISTRHO") != std::string::npos;
    
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
            
            // DPF plugins might have specific channel requirements
            if (isDPFPlugin) {
                DEBUG_PRINT("DPF plugin - trying default bus layout...");
                // Let DPF plugin use its preferred layout
            }
        }
    }
    
    // DPF-specific preparation
    if (isDPFPlugin) {
        DEBUG_PRINT("Applying DPF-specific preparation for '" << name << "'");
        
        // Ensure processing is enabled
        plugin->suspendProcessing(false);

        // Set precision (DPF usually works better with single precision)
        plugin->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
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
        
        // DPF plugins often need exact channel matching
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