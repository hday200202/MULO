#include "Effect.hpp"
#include "VSTPluginManager.hpp"
#include "../DebugConfig.hpp"
#include <thread>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <mutex>

#ifdef _WIN32
#undef max
#undef min
#endif

std::vector<std::unique_ptr<juce::AudioPluginInstance>> Effect::scheduledPlugins;
std::mutex Effect::cleanupMutex;
std::unordered_map<std::string, int> Effect::pluginInstanceCount;

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
        
        processor->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
        
        auto* editor = processor->createEditor();
        if (editor) {
            setContentOwned(editor, true);
            
            int editorWidth = editor->getWidth();
            int editorHeight = editor->getHeight();
            
            if (isDPFPlugin && (editorWidth < 100 || editorHeight < 100)) {
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
            
            repaint();
        } else {
            std::cerr << "Failed to create VST editor" << std::endl;
        }
    } else {
        std::cerr << "VST has no editor or processor is null" << std::endl;
    }
}

VSTEditorWindow::~VSTEditorWindow() {
    setContentOwned(nullptr, false);
    setVisible(false);
}

void VSTEditorWindow::closeButtonPressed() {
    setVisible(false);
    if (closeCallback) {
        closeCallback();
    }
}

Effect::~Effect() {
    try {
        if (editorWindow) {
            editorWindow->setVisible(false);
            editorWindow.reset();
        }
        
        if (plugin) {
            try {
                plugin->suspendProcessing(true);
                
                if (plugin->hasEditor()) {
                    auto* editor = plugin->getActiveEditor();
                    if (editor) {
                    plugin->editorBeingDeleted(editor);
                }
                }
                
                plugin->releaseResources();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                
            } catch (...) {
                std::cerr << "Unknown exception during plugin preparation for destruction" << std::endl;
            }
            
            std::string pluginName = plugin->getName().toStdString();
            bool isLastInstance = false;
            {
                std::lock_guard<std::mutex> lock(cleanupMutex);
                auto it = pluginInstanceCount.find(pluginName);
                if (it != pluginInstanceCount.end() && it->second > 0) {
                    it->second--;
                    isLastInstance = (it->second == 0);
                }
            }
            
            bool isProblematicPlugin = (pluginName.find("Zebra2") != std::string::npos) || 
                                     (pluginName.find("zebra") != std::string::npos);
            
            if (!scheduledForCleanup) {
                if (isLastInstance || isProblematicPlugin) {
                    try {
                        plugin->suspendProcessing(true);
                        plugin->setPlayHead(nullptr);
                        
                        if (!isProblematicPlugin && plugin->hasEditor()) {
                            auto* editor = plugin->getActiveEditor();
                            if (editor) {
                                plugin->editorBeingDeleted(editor);
                            }
                        }
                        plugin->releaseResources();
                    } catch (...) {
                        plugin.reset();
                    }
                    plugin.release();
                } else {
                    scheduledForCleanup = true;
                    
                    try {
                        if (plugin) {
                            plugin->suspendProcessing(true);
                            plugin->setPlayHead(nullptr);
                            
                            if (isSynthesizer()) {
                                juce::AudioBuffer<float> silentBuffer(2, 256);
                                silentBuffer.clear();
                                juce::MidiBuffer allNotesOff;
                                
                                for (int channel = 1; channel <= 16; ++channel) {
                                    allNotesOff.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
                                    allNotesOff.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0);
                                }
                                
                                plugin->processBlock(silentBuffer, allNotesOff);
                            }
                            
                            if (plugin->hasEditor()) {
                                try {
                                    auto* editor = plugin->getActiveEditor();
                                    if (editor) {
                                        plugin->editorBeingDeleted(editor);
                                    }
                                } catch (...) {
                                }
                            }
                            
                            
                            plugin->releaseResources();
                            plugin->reset();
                        }
                        
                        plugin.reset();
                        
                    } catch (...) {
                        plugin.reset();
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during Effect destruction (" << name << "): " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception during Effect destruction: " << name << std::endl;
    }
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
    
    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();
    
    juce::File vstFile(vstPath);
    if (!vstFile.exists()) {
        std::cerr << "VST file does not exist: " << vstPath << std::endl;
        return false;
    }
    
    juce::OwnedArray<juce::PluginDescription> descriptions;
    
    for (auto* format : formatManager.getFormats()) {
        format->findAllTypesForFile(descriptions, vstFile.getFullPathName());
        if (!descriptions.isEmpty()) {
            break;
        }
    }
    
    if (descriptions.isEmpty()) {
        std::cerr << "No valid plugin found in file: " << vstPath << std::endl;
        return false;
    }
    
    auto* description = descriptions[0];
    bool isDPFPlugin = description->manufacturerName.toLowerCase().contains("distrho") || 
                       description->manufacturerName.toLowerCase().contains("dpf") ||
                       description->category.toLowerCase().contains("dpf");
    
    juce::String errorMessage;
    
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
        plugin->suspendProcessing(false);
        plugin->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
    } else {
        plugin->suspendProcessing(false);
    }
    
    std::string pluginName = plugin->getName().toStdString();
    {
        std::lock_guard<std::mutex> lock(cleanupMutex);
        pluginInstanceCount[pluginName]++;
    }

    hasEditorCached = plugin->hasEditor();

    return true;
}

void Effect::prepareToPlay(double sampleRate, int bufferSize) {
    if (!plugin) return;
    
    bool isDPFPlugin = plugin->getName().toLowerCase().contains("dpf") ||
                       name.find("DPF") != std::string::npos ||
                       name.find("DISTRHO") != std::string::npos;
    
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    
    if (!plugin->setBusesLayout(layout)) {
        layout.inputBuses.clear();
        layout.outputBuses.clear();
        layout.inputBuses.add(juce::AudioChannelSet::mono());
        layout.outputBuses.add(juce::AudioChannelSet::mono());
        
        if (!plugin->setBusesLayout(layout)) {
            if (isDPFPlugin) {
            }
        }
    }
    
    if (isDPFPlugin) {
        plugin->suspendProcessing(false);
        plugin->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
    }
    
    plugin->prepareToPlay(sampleRate, bufferSize);
}

void Effect::processAudio(juce::AudioBuffer<float>& buffer) {
    if (!plugin || !isEnabled || scheduledForCleanup) {
        return;
    }
    
    if (isSynthesizer() && isSilenced()) {
        buffer.clear();
        return;
    }
    
    try {
        if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) {
            return;
        }
        
        int pluginInputChannels = plugin->getTotalNumInputChannels();
        int pluginOutputChannels = plugin->getTotalNumOutputChannels();
        int bufferChannels = buffer.getNumChannels();
        
        bool isDPFPlugin = plugin->getName().toLowerCase().contains("dpf") ||
                           name.find("DPF") != std::string::npos ||
                           name.find("DISTRHO") != std::string::npos;
        
        juce::AudioBuffer<float> processBuffer;
        if (pluginOutputChannels != bufferChannels || pluginInputChannels != bufferChannels) {
            int maxChannels = (std::max)({pluginInputChannels, pluginOutputChannels, bufferChannels});
            processBuffer.setSize(maxChannels, buffer.getNumSamples());
            processBuffer.clear();
            
            for (int ch = 0; ch < (std::min)(bufferChannels, pluginInputChannels); ++ch) {
                processBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }
            
            juce::MidiBuffer midiBuffer;
            
            if (isDPFPlugin) {
                midiBuffer.clear();
            }
            
            plugin->processBlock(processBuffer, midiBuffer);
            
            for (int ch = 0; ch < (std::min)(bufferChannels, pluginOutputChannels); ++ch) {
                buffer.copyFrom(ch, 0, processBuffer, ch, 0, buffer.getNumSamples());
            }
        } else {
            juce::MidiBuffer midiBuffer;
            
            if (isDPFPlugin) {
                midiBuffer.clear();
            }
            
            plugin->processBlock(buffer, midiBuffer);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: VST '" << name << "' crashed during audio processing: " << e.what() << std::endl;
        isEnabled = false; 
    } catch (...) {
        std::cerr << "ERROR: VST '" << name << "' crashed during audio processing (unknown exception)" << std::endl;
        isEnabled = false; 
    }
}

void Effect::processAudio(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiBuffer) {
    if (!plugin || !isEnabled || scheduledForCleanup) {
        return;
    }

    if (isSynthesizer() && isSilenced()) {
        buffer.clear(); 
        return;
    }
    
    try {
        if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) {
            return;
        }

        int pluginInputChannels = plugin->getTotalNumInputChannels();
        int pluginOutputChannels = plugin->getTotalNumOutputChannels();
        int bufferChannels = buffer.getNumChannels();

        juce::AudioBuffer<float> processBuffer;
        if (pluginOutputChannels != bufferChannels || pluginInputChannels != bufferChannels) {
            
            int maxChannels = (std::max)({pluginInputChannels, pluginOutputChannels, bufferChannels});
            processBuffer.setSize(maxChannels, buffer.getNumSamples());
            processBuffer.clear();
 
            for (int ch = 0; ch < (std::min)(bufferChannels, pluginInputChannels); ++ch) {
                processBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
            }

            plugin->processBlock(processBuffer, midiBuffer);

            for (int ch = 0; ch < (std::min)(bufferChannels, pluginOutputChannels); ++ch) {
                buffer.copyFrom(ch, 0, processBuffer, ch, 0, buffer.getNumSamples());
            }
        } else {
            
            plugin->processBlock(buffer, midiBuffer);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: VST '" << name << "' crashed during audio processing: " << e.what() << std::endl;
        isEnabled = false; 
    } catch (...) {
        std::cerr << "ERROR: VST '" << name << "' crashed during audio processing (unknown exception)" << std::endl;
        isEnabled = false; 
    }
}

void Effect::openWindow() {
    if (!plugin || !hasEditorCached) {
        std::cout << "Cannot open window - plugin: " << (plugin ? "OK" : "NULL") 
                  << ", hasEditor: " << (hasEditorCached ? "YES" : "NO") << std::endl;
        return;
    }
    
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
        std::cout << "Not on message thread, dispatching to main thread" << std::endl;
        juce::MessageManager::callAsync([this]() {
            openWindow();
        });
        return;
    }
    
    if (editorWindow) {
        editorWindow.reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!plugin->hasEditor()) {
        std::cout << "Plugin no longer has editor capability: " << name << std::endl;
        return;
    }

    try {
        editorWindow = std::make_unique<VSTEditorWindow>(juce::String(getName()), plugin.get(), [this]() {
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

std::string Effect::getParameterName(int index) const {
    if (!plugin) return "";
    
    const auto& parameters = plugin->getParameters();
    if (index >= 0 && index < static_cast<int>(parameters.size())) {
        return parameters[index]->getName(256).toStdString(); // 256 is max length
    }
    
    return "";
}

const juce::Array<juce::AudioProcessorParameter *>& Effect::getAllParameters() const {
    if (!plugin) return {};

    return plugin->getParameters();
}

int Effect::getNumParameters() const {
    if (!plugin) return 0;
    return plugin->getParameters().size();
}

void Effect::resetBuffers() {
    if (!plugin) return;
    
    try {
        if (isSynthesizer()) {
            setSilenced(true);
            
            juce::AudioBuffer<float> tempBuffer(2, 256);
            tempBuffer.clear();
            juce::MidiBuffer resetMidiBuffer;
            
            for (int channel = 1; channel <= 16; ++channel) {
                resetMidiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0); 
                resetMidiBuffer.addEvent(juce::MidiMessage::allNotesOff(channel), 0); 
                resetMidiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 121, 0), 0); 
                resetMidiBuffer.addEvent(juce::MidiMessage::controllerEvent(channel, 123, 0), 0); 
            }

            for (int i = 0; i < 3; ++i) {
                tempBuffer.clear(); 
                plugin->processBlock(tempBuffer, resetMidiBuffer);
                resetMidiBuffer.clear(); 
            }

            plugin->reset();

            juce::MidiBuffer emptyMidi;
            for (int cycle = 0; cycle < 5; ++cycle) {
                tempBuffer.clear(); 
                plugin->processBlock(tempBuffer, emptyMidi);
            }
        } else {
            plugin->reset();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to reset buffers for VST '" << name << "': " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "ERROR: Failed to reset buffers for VST '" << name << "' (unknown exception)" << std::endl;
    }
}

void Effect::setBpm(double bpm) {
    if (!plugin || !isSynthesizer()) {
        return;
    }
    
    try {} catch (const std::exception& e) {}
}

void Effect::setPlayHead(juce::AudioPlayHead* playHead) {
    if (!plugin) {
        return;
    }
    
    try {
        plugin->setPlayHead(playHead);
    } catch (const std::exception& e) {
        
    }
}

bool Effect::isSynthesizer() const {
    if (!plugin) return false;

    if (synthesizerCached) {
        return isSynthesizerCached;
    }

    bool acceptsMidi = plugin->acceptsMidi();

    int audioInputChannels = plugin->getTotalNumInputChannels();
    int audioOutputChannels = plugin->getTotalNumOutputChannels();

    bool hasAudioOutput = audioOutputChannels > 0;
    bool hasMinimalAudioInput = audioInputChannels <= 0;

    bool isInstrumentCategory = false;
    try {
        juce::String category = plugin->getPluginDescription().category.toLowerCase();
        isInstrumentCategory = category.contains("instrument") || 
                              category.contains("synth") || 
                              category.contains("generator");
    } catch (...) {
        isInstrumentCategory = false;
    }

    bool isSynth = acceptsMidi && hasAudioOutput && (hasMinimalAudioInput || isInstrumentCategory);

    isSynthesizerCached = isSynth;
    synthesizerCached = true;
    
    return isSynth;
}

bool Effect::isVSTSynthesizer(const std::string& vstPath) {
    auto& vstManager = VSTPluginManager::getInstance();
    if (!vstManager.isValidVSTFile(vstPath)) {
        return false;
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();
    
    juce::File vstFile(vstPath);
    if (!vstFile.exists()) {
        return false;
    }
    
    juce::OwnedArray<juce::PluginDescription> descriptions;
    for (auto* format : formatManager.getFormats()) {
        format->findAllTypesForFile(descriptions, vstFile.getFullPathName());
        if (!descriptions.isEmpty()) {
            break;
        }
    }
    
    if (descriptions.isEmpty()) {
        return false;
    }
    
    auto* description = descriptions[0];
    
    juce::String errorMessage;
    auto plugin = formatManager.createPluginInstance(*description, 44100.0, 512, errorMessage);
    
    if (!plugin) {
        return false;
    }
    
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
    
    return isSynth;
}

void Effect::scheduleForCleanup() {
    if (plugin && !scheduledForCleanup) {
        scheduledForCleanup = true;
        
        try {
            plugin->suspendProcessing(true);
            plugin->setPlayHead(nullptr);
            
            if (isSynthesizer()) {
                juce::AudioBuffer<float> silentBuffer(2, 256);
                silentBuffer.clear();
                juce::MidiBuffer allNotesOff;
                
                for (int channel = 1; channel <= 16; ++channel) {
                    allNotesOff.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
                    allNotesOff.addEvent(juce::MidiMessage::controllerEvent(channel, 120, 0), 0);
                }
                
                plugin->processBlock(silentBuffer, allNotesOff);
            }
            
            if (plugin->hasEditor()) {
                auto* editor = plugin->getActiveEditor();
                if (editor) {
                    plugin->editorBeingDeleted(editor);
                }
            }
            
            plugin->releaseResources();
            plugin->reset();
        } catch (...) {}
                
        plugin.reset();
    }
}

void Effect::cleanupScheduledPlugins() {
    static std::atomic<bool> cleanupInProgress{false};
    
    if (cleanupInProgress.exchange(true)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(cleanupMutex);
    
    if (scheduledPlugins.empty()) {
        cleanupInProgress = false;
        return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    size_t originalSize = scheduledPlugins.size();
    size_t cleaned = 0;
    size_t failed = 0;
    
    auto it = scheduledPlugins.begin();
    while (it != scheduledPlugins.end()) {
        try {
            if (!*it) {
                it = scheduledPlugins.erase(it);
                continue;
            }
            
            auto* plugin = it->get();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            plugin->suspendProcessing(true);
            plugin->setPlayHead(nullptr);
            
            if (plugin->hasEditor()) {
                try {
                    if (auto* editor = plugin->getActiveEditor()) {
                        plugin->editorBeingDeleted(editor);
                    }
                } catch (const std::exception& e) {} catch (...) {}
            }
            
            plugin->releaseResources();
            plugin->reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            it = scheduledPlugins.erase(it);
            cleaned++;
            
        } catch (const std::exception& e) {
            ++it;
            failed++;
        } catch (...) {
            ++it;
            failed++;
        }
    }
    
    cleanupInProgress = false;
}
