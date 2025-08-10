#include "Engine.hpp"
#include "../DebugConfig.hpp"
#include <chrono>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Engine::Engine() {
    formatManager.registerBasicFormats();
    
    deviceManager.initialise(0, 2, nullptr, false);
    
    // Don't force a specific sample rate here - let the application configure it
    // after loading the config file
    auto currentSetup = deviceManager.getAudioDeviceSetup();
    
    currentSetup.bufferSize = 256;
    
    juce::String error = deviceManager.setAudioDeviceSetup(currentSetup, true);
    
    if (error.isNotEmpty()) {
        currentSetup.bufferSize = 512;
        error = deviceManager.setAudioDeviceSetup(currentSetup, true);
        
        if (error.isNotEmpty()) {
            currentSetup.bufferSize = 1024;
            error = deviceManager.setAudioDeviceSetup(currentSetup, true);
            
            if (error.isNotEmpty()) {
                DEBUG_PRINT("Warning: Could not set low-latency audio buffer. Using default settings.");
                DEBUG_PRINT("Audio setup error: " << error.toStdString());
            }
        }
    }
    
    auto finalSetup = deviceManager.getAudioDeviceSetup();
    DEBUG_PRINT("Audio device setup (initial):");
    DEBUG_PRINT("  Sample rate: " << finalSetup.sampleRate << " Hz");
    DEBUG_PRINT("  Buffer size: " << finalSetup.bufferSize << " samples");
    DEBUG_PRINT_INLINE("  Latency: ~" << std::fixed << std::setprecision(1) 
              << (finalSetup.bufferSize / finalSetup.sampleRate * 1000.0) << " ms");
    
    deviceManager.addAudioCallback(this);

    masterTrack = std::make_unique<Track>(formatManager);
    masterTrack->setName("Master");
    
    selectedTrackName = "Master";
}

bool Engine::configureAudioDevice(double desiredSampleRate, int bufferSize) {
    DEBUG_PRINT("Configuring audio device: " << desiredSampleRate << " Hz, " << bufferSize << " samples");
    
    auto currentSetup = deviceManager.getAudioDeviceSetup();
    currentSetup.sampleRate = desiredSampleRate;
    currentSetup.bufferSize = bufferSize;
    
    juce::String error = deviceManager.setAudioDeviceSetup(currentSetup, true);
    
    if (error.isNotEmpty()) {
        DEBUG_PRINT("Failed to set desired audio setup, trying fallback buffer sizes...");
        DEBUG_PRINT("Error: " << error.toStdString());
        
        // Try with larger buffer sizes if the desired one fails
        std::vector<int> fallbackBuffers = {512, 1024, 2048};
        for (int fallbackBuffer : fallbackBuffers) {
            currentSetup.bufferSize = fallbackBuffer;
            error = deviceManager.setAudioDeviceSetup(currentSetup, true);
            
            if (error.isEmpty()) {
                DEBUG_PRINT("Successfully set audio device with fallback buffer: " << fallbackBuffer);
                break;
            }
        }
        
        if (error.isNotEmpty()) {
            DEBUG_PRINT("Failed to configure audio device with desired sample rate: " << desiredSampleRate);
            DEBUG_PRINT("Final error: " << error.toStdString());
            return false;
        }
    }
    
    auto finalSetup = deviceManager.getAudioDeviceSetup();
    DEBUG_PRINT("Audio device configured:");
    DEBUG_PRINT("  Sample rate: " << finalSetup.sampleRate << " Hz");
    DEBUG_PRINT("  Buffer size: " << finalSetup.bufferSize << " samples");
    DEBUG_PRINT_INLINE("  Latency: ~" << std::fixed << std::setprecision(1) 
              << (finalSetup.bufferSize / finalSetup.sampleRate * 1000.0) << " ms");
              
    // Update internal sample rate to match what was actually set
    sampleRate = finalSetup.sampleRate;
    currentBufferSize = finalSetup.bufferSize;
    
    return true;
}

Engine::~Engine() {
    deviceManager.closeAudioDevice();
    deviceManager.removeAudioCallback(this);
}

void Engine::exportMaster(const std::string& filePath) {
    double startTime = std::numeric_limits<double>::max();
    double endTime = 0.0;

    for (const auto& track : currentComposition->tracks) {
        for (const auto& clip : track->getClips()) {
            if (clip.startTime < startTime) startTime = clip.startTime;
            double clipEnd = clip.startTime + clip.duration;
            if (clipEnd > endTime) endTime = clipEnd;
        }
    }
    if (startTime == std::numeric_limits<double>::max()) startTime = 0.0;

    int sampleRate = static_cast<int>(getSampleRate());
    int numChannels = 2; // Stereo
    int startSample = static_cast<int>(startTime * sampleRate);
    int endSample = static_cast<int>(endTime * sampleRate);
    int totalSamples = endSample - startSample;

    juce::AudioBuffer<float> outputBuffer(numChannels, totalSamples);

    setPosition(startTime);

    int blockSize = 512;
    for (int pos = 0; pos < totalSamples; pos += blockSize) {
        int samplesToProcess = std::min(blockSize, totalSamples - pos);
        juce::AudioBuffer<float> tempBlock(numChannels, samplesToProcess);
        processBlock(tempBlock, samplesToProcess);
        for (int ch = 0; ch < numChannels; ++ch)
            outputBuffer.copyFrom(ch, pos, tempBlock, ch, 0, samplesToProcess);
        positionSeconds += samplesToProcess / static_cast<double>(sampleRate);
    }

    // Write to WAV using JUCE
    juce::String basePath = juce::String(filePath);
    if (!basePath.endsWithChar('/') && !basePath.endsWithChar('\\'))
        basePath += juce::File::getSeparatorString();
    juce::String fileName = currentComposition->name;
    if (!fileName.endsWithIgnoreCase(".wav"))
        fileName += ".wav";
    juce::File outFile(basePath + fileName);
    outFile = outFile.getNonexistentSibling(); // Avoid overwrite
    outFile.getParentDirectory().createDirectory(); // Ensure directory exists

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> stream(outFile.createOutputStream());
    if (stream) {
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(stream.get(), sampleRate, numChannels, 16, {}, 0));
        if (writer) {
            writer->writeFromAudioSampleBuffer(outputBuffer, 0, totalSamples);
        }
        stream.release();
    }
}

// Directory management
void Engine::setVSTDirectory(const std::string& directory) {
    vstDirectory = directory;
    DEBUG_PRINT("VST directory set to: " + directory);
}

void Engine::setSampleDirectory(const std::string& directory) {
    sampleDirectory = directory;
    DEBUG_PRINT("Sample directory set to: " + directory);
}

std::string Engine::getVSTDirectory() const {
    return vstDirectory;
}

std::string Engine::getSampleDirectory() const {
    return sampleDirectory;
}

juce::File Engine::findSampleFile(const std::string& sampleName) const {
    if (sampleDirectory.empty() || sampleName.empty()) {
        return juce::File();
    }
    
    juce::File directory(sampleDirectory);
    if (!directory.exists() || !directory.isDirectory()) {
        DEBUG_PRINT("Sample directory does not exist: " + sampleDirectory);
        return juce::File();
    }
    
    // First try exact filename match (with extension)
    juce::File exactFile = directory.getChildFile(sampleName);
    if (exactFile.existsAsFile()) {
        DEBUG_PRINT("Found sample file (exact match): " + exactFile.getFullPathName().toStdString());
        return exactFile;
    }
    
    // If no exact match, try adding common audio file extensions
    std::vector<std::string> extensions = {".wav", ".mp3", ".flac", ".ogg", ".aiff", ".m4a"};
    
    for (const auto& ext : extensions) {
        juce::File file = directory.getChildFile(sampleName + ext);
        if (file.existsAsFile()) {
            DEBUG_PRINT("Found sample file (with extension): " + file.getFullPathName().toStdString());
            return file;
        }
    }
    
    // If still no match, try case-insensitive search
    juce::Array<juce::File> allFiles;
    directory.findChildFiles(allFiles, juce::File::findFiles, false);
    
    for (const auto& file : allFiles) {
        std::string fileName = file.getFileName().toStdString();
        std::string targetName = sampleName;
        
        // Convert to lowercase for comparison
        std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
        std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
        
        if (fileName == targetName) {
            DEBUG_PRINT("Found sample file (case-insensitive): " + file.getFullPathName().toStdString());
            return file;
        }
    }
    
    DEBUG_PRINT("Sample file not found: " + sampleName + " in directory: " + sampleDirectory);
    return juce::File();
}

juce::File Engine::findVSTFile(const std::string& vstName) const {
    if (vstDirectory.empty() || vstName.empty()) {
        return juce::File();
    }
    
    juce::File directory(vstDirectory);
    if (!directory.exists() || !directory.isDirectory()) {
        DEBUG_PRINT("VST directory does not exist: " + vstDirectory);
        return juce::File();
    }
    
    // First try exact filename match (with extension)
    juce::File exactFile = directory.getChildFile(vstName);
    if (exactFile.existsAsFile()) {
        DEBUG_PRINT("Found VST file (exact match): " + exactFile.getFullPathName().toStdString());
        return exactFile;
    }
    
    // If no exact match, try adding common VST file extensions
    std::vector<std::string> extensions = {".dll", ".vst", ".vst3"};
    
    for (const auto& ext : extensions) {
        juce::File file = directory.getChildFile(vstName + ext);
        if (file.existsAsFile()) {
            DEBUG_PRINT("Found VST file (with extension): " + file.getFullPathName().toStdString());
            return file;
        }
    }
    
    // If still no match, try case-insensitive search
    juce::Array<juce::File> allFiles;
    directory.findChildFiles(allFiles, juce::File::findFiles, false);
    
    for (const auto& file : allFiles) {
        std::string fileName = file.getFileName().toStdString();
        std::string targetName = vstName;
        
        // Convert to lowercase for comparison
        std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
        std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
        
        if (fileName == targetName) {
            DEBUG_PRINT("Found VST file (case-insensitive): " + file.getFullPathName().toStdString());
            return file;
        }
    }
    
    DEBUG_PRINT("VST file not found: " + vstName + " in directory: " + vstDirectory);
    return juce::File();
}

void Engine::play() {
    if (hasSaved) {
        positionSeconds = savedPosition;
        hasSaved = false; // Clear saved position after using it
    }
    playing = true;
}

void Engine::pause() {
    playing = false;
}

void Engine::stop() {
    playing = false;
    positionSeconds = 0.0;
}

void Engine::setPosition(double s) {
    positionSeconds = juce::jmax(0.0, s);
}

double Engine::getPosition() const {
    return positionSeconds;
}

bool Engine::isPlaying() const {
    return playing;
}

void Engine::setSavedPosition(double seconds) {
    savedPosition = juce::jmax(0.0, seconds);
    hasSaved = true;
}

double Engine::getSavedPosition() const {
    return savedPosition;
}

bool Engine::hasSavedPosition() const {
    return hasSaved;
}

void Engine::newComposition(const std::string& name) {
    currentComposition = std::make_unique<Composition>();
    currentComposition->name = name;
}

void Engine::loadComposition(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open composition file: " << path << "\n";
        return;
    }

    // Read entire file content
    std::ostringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    std::string content = buffer.str();
    
    // Use loadState to parse the JSON format
    DEBUG_PRINT("Loading project file: " << path);
    loadState(content);
}

void Engine::saveComposition(const std::string&) {}

std::pair<int, int> Engine::getTimeSignature() const {
    if (!currentComposition) {
        return {4, 4}; // Default time signature
    }
    return {currentComposition->timeSigNumerator, currentComposition->timeSigDenominator};
}

double Engine::getBpm() const { 
    if (!currentComposition) {
        return 120.0; // Default BPM
    }
    return currentComposition->bpm; 
}

void Engine::setBpm(double newBpm) { 
    if (currentComposition) {
        currentComposition->bpm = newBpm; 
    }
}

void Engine::addTrack(const std::string& name, const std::string& samplePath) {
    if (!currentComposition) {
        currentComposition = std::make_unique<Composition>();
        currentComposition->name = "untitled";
    }
    
    std::string baseName = name;
    std::string uniqueName = baseName;
    int suffix = 1;
    auto nameExists = [&](const std::string& n) {
        for (const auto& track : currentComposition->tracks) {
            if (track && track->getName() == n) return true;
        }
        return false;
    };
    while (nameExists(uniqueName)) {
        uniqueName = baseName + "_" + std::to_string(suffix++);
    }

    auto t = std::make_unique<Track>(formatManager);
    t->setName(uniqueName);
    
    t->prepareToPlay(sampleRate, currentBufferSize);

    if (!samplePath.empty() && uniqueName != "Master") {
        juce::File sampleFile(samplePath);
        double lengthSeconds = 2.0;
        if (auto* reader = formatManager.createReaderFor(sampleFile)) {
            lengthSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
            delete reader;
        }
        t->setReferenceClip({sampleFile, 0.0, 0.0, lengthSeconds, 1.0f});
    }

    currentComposition->tracks.push_back(std::move(t));
}

void Engine::removeTrack(int idx) {
    if (currentComposition && idx >= 0 && idx < currentComposition->tracks.size()) {
        currentComposition->tracks.erase(currentComposition->tracks.begin() + idx);
    }
}

void Engine::removeTrackByName(const std::string& name) {
    if (currentComposition) {
        for (int i = 0; i < currentComposition->tracks.size(); i++)
            if (currentComposition->tracks[i]->getName() == name)
                currentComposition->tracks.erase(currentComposition->tracks.begin() + i);
    }
}

std::string Engine::getCurrentCompositionName() const {
    return currentComposition ? currentComposition->name : "untitled";
}

void Engine::setCurrentCompositionName(const std::string& newName) {
    if (currentComposition) {
        currentComposition->name = newName;
    }
}

Track* Engine::getTrack(int idx) {
    if (currentComposition && idx >= 0 && idx < currentComposition->tracks.size()) {
        return currentComposition->tracks[idx].get();
    }
    return nullptr;
}

Track* Engine::getTrackByName(const std::string& name) {
    if (masterTrack && masterTrack->getName() == name) {
        return masterTrack.get();
    }
    if (currentComposition) {
        for (const auto& track : currentComposition->tracks) {
            if (track && track->getName() == name) {
                return track.get();
            }
        }
    }
    return nullptr;
}

std::vector<std::unique_ptr<Track>>& Engine::getAllTracks() {
    if (!currentComposition) {
        // Create empty composition if none exists to avoid null pointer access
        static std::vector<std::unique_ptr<Track>> emptyTracks;
        return emptyTracks;
    }
    return currentComposition->tracks;
}

Track* Engine::getMasterTrack() {
    return masterTrack.get();
}

void Engine::setSelectedTrack(const std::string& trackName) {
    // Validate that the track exists (including Master track)
    if (trackName == "Master") {
        selectedTrackName = trackName;
        return;
    }
    
    // Check if track exists in composition
    if (currentComposition) {
        for (const auto& track : currentComposition->tracks) {
            if (track->getName() == trackName) {
                selectedTrackName = trackName;
                return;
            }
        }
    }
    
    std::cerr << "[Engine] Warning: Track '" << trackName << "' does not exist" << std::endl;
}

std::string Engine::getSelectedTrack() const {
    return selectedTrackName;
}

Track* Engine::getSelectedTrackPtr() {
    if (selectedTrackName.empty()) {
        return nullptr;
    }
    
    if (selectedTrackName == "Master") {
        return masterTrack.get();
    }
    
    if (currentComposition) {
        for (const auto& track : currentComposition->tracks) {
            if (track->getName() == selectedTrackName) {
                return track.get();
            }
        }
    }
    
    return nullptr;
}

bool Engine::hasSelectedTrack() const {
    return !selectedTrackName.empty();
}

void Engine::audioDeviceIOCallbackWithContext(
    const float* const*, int,
    float* const* outputChannelData, int numOutputChannels,
    int numSamples, const juce::AudioIODeviceCallbackContext&
) {
    juce::AudioBuffer<float> out(outputChannelData, numOutputChannels, numSamples);
    out.clear();

    if (playing) {
        processBlock(out, numSamples);
        positionSeconds += static_cast<double>(numSamples) / sampleRate;
    }
}

void Engine::saveState() {
    if (!currentComposition) {
        currentState = "";
        return;
    }
    
    json engineState;
    engineState["engineState"]["version"] = "1.0";
    engineState["engineState"]["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Playback state
    auto& playbackState = engineState["engineState"]["playbackState"];
    playbackState["playing"] = playing;
    playbackState["positionSeconds"] = positionSeconds;
    playbackState["savedPosition"] = savedPosition;
    playbackState["hasSaved"] = hasSaved;
    playbackState["selectedTrackName"] = selectedTrackName;
    
    // Audio device settings
    auto& audioSettings = engineState["engineState"]["audioSettings"];
    audioSettings["sampleRate"] = sampleRate;
    audioSettings["currentBufferSize"] = currentBufferSize;
    
    // Composition data
    auto& composition = engineState["engineState"]["composition"];
    composition["name"] = currentComposition->name;
    composition["bpm"] = currentComposition->bpm;
    composition["timeSignature"]["numerator"] = currentComposition->timeSigNumerator;
    composition["timeSignature"]["denominator"] = currentComposition->timeSigDenominator;
    
    // Master track
    auto& masterTrackJson = composition["masterTrack"];
    if (masterTrack) {
        masterTrackJson["name"] = masterTrack->getName();
        masterTrackJson["volume"] = masterTrack->getVolume();
        masterTrackJson["pan"] = masterTrack->getPan();
        masterTrackJson["muted"] = masterTrack->isMuted();
        masterTrackJson["soloed"] = masterTrack->isSolo();
        
        // Master track effects
        auto& masterEffects = masterTrackJson["effects"];
        const auto& masterEffectsList = masterTrack->getEffects();
        for (const auto& effect : masterEffectsList) {
            if (effect) {
                json effectJson;
                effectJson["name"] = effect->getName();
                // Extract filename with extension from the VST path
                juce::File vstFile(effect->getVSTPath());
                effectJson["vstName"] = vstFile.getFileName().toStdString();
                effectJson["enabled"] = effect->enabled();
                effectJson["index"] = effect->getIndex();
                
                // Effect parameters
                auto& parameters = effectJson["parameters"];
                int numParams = effect->getNumParameters();
                for (int p = 0; p < numParams; ++p) {
                    json paramJson;
                    paramJson["index"] = p;
                    paramJson["value"] = effect->getParameter(p);
                    parameters.push_back(paramJson);
                }
                masterEffects.push_back(effectJson);
            }
        }
    } else {
        masterTrackJson["name"] = "Master";
        masterTrackJson["volume"] = 0.0;
        masterTrackJson["pan"] = 0.0;
        masterTrackJson["muted"] = false;
        masterTrackJson["soloed"] = false;
        masterTrackJson["effects"] = json::array();
    }
    
    // All tracks
    auto& tracks = composition["tracks"];
    for (const auto& track : currentComposition->tracks) {
        if (!track) continue;
        
        json trackJson;
        trackJson["name"] = track->getName();
        trackJson["volume"] = track->getVolume();
        trackJson["pan"] = track->getPan();
        trackJson["muted"] = track->isMuted();
        trackJson["soloed"] = track->isSolo();
        
        // Reference clip
        auto* nonConstTrack = const_cast<Track*>(track.get());
        if (nonConstTrack->getReferenceClip()) {
            const auto* refClip = nonConstTrack->getReferenceClip();
            auto& refClipJson = trackJson["referenceClip"];
            refClipJson["file"] = refClip->sourceFile.getFileName().toStdString();
            refClipJson["startTime"] = refClip->startTime;
            refClipJson["offset"] = refClip->offset;
            refClipJson["duration"] = refClip->duration;
            refClipJson["volume"] = refClip->volume;
        } else {
            trackJson["referenceClip"] = nullptr;
        }
        
        // Track clips
        auto& clipsJson = trackJson["clips"];
        const auto& clips = track->getClips();
        for (const auto& clip : clips) {
            json clipJson;
            clipJson["file"] = clip.sourceFile.getFileName().toStdString();
            clipJson["startTime"] = clip.startTime;
            clipJson["offset"] = clip.offset;
            clipJson["duration"] = clip.duration;
            clipJson["volume"] = clip.volume;
            clipsJson.push_back(clipJson);
        }
        
        // Track effects
        auto& trackEffects = trackJson["effects"];
        const auto& trackEffectsList = const_cast<Track*>(track.get())->getEffects();
        for (const auto& effect : trackEffectsList) {
            if (effect) {
                json effectJson;
                effectJson["name"] = effect->getName();
                // Extract filename with extension from the VST path
                juce::File vstFile(effect->getVSTPath());
                effectJson["vstName"] = vstFile.getFileName().toStdString();
                effectJson["enabled"] = effect->enabled();
                effectJson["index"] = effect->getIndex();
                
                // Effect parameters
                auto& parameters = effectJson["parameters"];
                int numParams = effect->getNumParameters();
                for (int p = 0; p < numParams; ++p) {
                    json paramJson;
                    paramJson["index"] = p;
                    paramJson["value"] = effect->getParameter(p);
                    parameters.push_back(paramJson);
                }
                trackEffects.push_back(effectJson);
            }
        }
        
        tracks.push_back(trackJson);
    }

    currentState = engineState.dump(2); // Pretty print with 2-space indentation
    DEBUG_PRINT("Engine state serialized to currentState string");
}

void Engine::save(const std::string& path) const {
    if (currentState.empty()) {
        DEBUG_PRINT("Warning: No current state to save. Call saveState() first.");
        return;
    }
    
    std::ofstream out(path);
    if (!out.is_open()) {
        DEBUG_PRINT("Failed to open file for writing: " << path);
        return;
    }
    
    out << currentState;
    out.close();
    
    DEBUG_PRINT("Engine state written to file: " << path);
}

std::string Engine::getStateString() const {
    return currentState;
}

void Engine::loadState(const std::string& stateData) {
    if (stateData.empty()) {
        DEBUG_PRINT("Engine::loadState called with empty state string");
        return;
    }
    
    DEBUG_PRINT("Engine::loadState called with state size: " + std::to_string(stateData.size()));
    
    try {
        json parsedState = json::parse(stateData);
        
        if (!parsedState.contains("engineState")) {
            DEBUG_PRINT("ERROR: No engineState section found in JSON");
            return;
        }
        
        const auto& engineState = parsedState["engineState"];
        
        // Load playback state
        if (engineState.contains("playbackState")) {
            const auto& playback = engineState["playbackState"];
            if (playback.contains("playing")) {
                playing = playback["playing"].get<bool>();
            }
            if (playback.contains("positionSeconds")) {
                positionSeconds = playback["positionSeconds"].get<double>();
            }
            if (playback.contains("savedPosition")) {
                savedPosition = playback["savedPosition"].get<double>();
            }
            if (playback.contains("hasSaved")) {
                hasSaved = playback["hasSaved"].get<bool>();
            }
            if (playback.contains("selectedTrackName")) {
                selectedTrackName = playback["selectedTrackName"].get<std::string>();
            }
        }
        
        // Load audio settings
        if (engineState.contains("audioSettings")) {
            const auto& audio = engineState["audioSettings"];
            if (audio.contains("sampleRate")) {
                sampleRate = audio["sampleRate"].get<double>();
            }
            if (audio.contains("currentBufferSize")) {
                currentBufferSize = audio["currentBufferSize"].get<int>();
            }
        }
        
        // Load composition
        if (engineState.contains("composition")) {
            const auto& composition = engineState["composition"];
            
            // Create a new composition or reuse existing
            if (!currentComposition) {
                currentComposition = std::make_unique<Composition>();
            }
            
            if (composition.contains("name")) {
                currentComposition->name = composition["name"].get<std::string>();
            }
            if (composition.contains("bpm")) {
                currentComposition->bpm = composition["bpm"].get<double>();
            }
            if (composition.contains("timeSignature")) {
                const auto& timeSig = composition["timeSignature"];
                if (timeSig.contains("numerator")) {
                    currentComposition->timeSigNumerator = timeSig["numerator"].get<int>();
                }
                if (timeSig.contains("denominator")) {
                    currentComposition->timeSigDenominator = timeSig["denominator"].get<int>();
                }
            }
            
            // Load master track
            if (composition.contains("masterTrack")) {
                const auto& masterTrackData = composition["masterTrack"];
                
                if (!masterTrack) {
                    masterTrack = std::make_unique<Track>(formatManager);
                }
                
                if (masterTrackData.contains("name")) {
                    masterTrack->setName(masterTrackData["name"].get<std::string>());
                }
                if (masterTrackData.contains("volume")) {
                    masterTrack->setVolume(masterTrackData["volume"].get<float>());
                }
                if (masterTrackData.contains("pan")) {
                    masterTrack->setPan(masterTrackData["pan"].get<float>());
                }
                if (masterTrackData.contains("muted")) {
                    bool shouldMute = masterTrackData["muted"].get<bool>();
                    if (shouldMute != masterTrack->isMuted()) {
                        masterTrack->toggleMute();
                    }
                }
                if (masterTrackData.contains("soloed")) {
                    masterTrack->setSolo(masterTrackData["soloed"].get<bool>());
                }
                
                // Load master track effects
                if (masterTrackData.contains("effects") && masterTrackData["effects"].is_array()) {
                    masterTrack->clearEffects();
                    for (const auto& effectData : masterTrackData["effects"]) {
                        if (effectData.contains("vstName")) {
                            std::string vstName = effectData["vstName"].get<std::string>();
                            
                            // Find VST file by name
                            juce::File vstFile = findVSTFile(vstName);
                            if (vstFile.existsAsFile()) {
                                // Store effect for deferred loading to prevent blank window issues
                                PendingEffect pendingEffect;
                                pendingEffect.trackName = "Master";
                                pendingEffect.vstPath = vstFile.getFullPathName().toStdString();
                                
                                if (effectData.contains("enabled")) {
                                    pendingEffect.enabled = effectData["enabled"].get<bool>();
                                }
                                if (effectData.contains("index")) {
                                    pendingEffect.index = effectData["index"].get<int>();
                                }
                                
                                // Extract parameters
                                if (effectData.contains("parameters") && effectData["parameters"].is_array()) {
                                    for (const auto& paramData : effectData["parameters"]) {
                                        if (paramData.contains("index") && paramData.contains("value")) {
                                            int paramIndex = paramData["index"].get<int>();
                                            float paramValue = paramData["value"].get<float>();
                                            pendingEffect.parameters.emplace_back(paramIndex, paramValue);
                                        }
                                    }
                                }
                                
                                pendingEffects.push_back(pendingEffect);
                                DEBUG_PRINT("Queued effect for deferred loading: " + vstName + " for Master track");
                            } else {
                                DEBUG_PRINT("VST file not found for master track effect: " + vstName);
                            }
                        }
                    }
                }
            }
            
            // Load tracks
            if (composition.contains("tracks") && composition["tracks"].is_array()) {
                currentComposition->tracks.clear();
                
                for (const auto& trackData : composition["tracks"]) {
                    if (!trackData.contains("name")) continue;
                    
                    std::string trackName = trackData["name"].get<std::string>();
                    auto track = std::make_unique<Track>(formatManager);
                    track->setName(trackName);
                    
                    if (trackData.contains("volume")) {
                        track->setVolume(trackData["volume"].get<float>());
                    }
                    if (trackData.contains("pan")) {
                        track->setPan(trackData["pan"].get<float>());
                    }
                    if (trackData.contains("muted")) {
                        bool shouldMute = trackData["muted"].get<bool>();
                        if (shouldMute != track->isMuted()) {
                            track->toggleMute();
                        }
                    }
                    if (trackData.contains("soloed")) {
                        track->setSolo(trackData["soloed"].get<bool>());
                    }
                    
                    // Load reference clip
                    if (trackData.contains("referenceClip") && !trackData["referenceClip"].is_null()) {
                        const auto& refClipData = trackData["referenceClip"];
                        if (refClipData.contains("file")) {
                            std::string fileName = refClipData["file"].get<std::string>();
                            juce::File file = findSampleFile(fileName);
                            
                            if (file.existsAsFile()) {
                                AudioClip refClip;
                                refClip.sourceFile = file;
                                if (refClipData.contains("startTime")) {
                                    refClip.startTime = refClipData["startTime"].get<double>();
                                }
                                if (refClipData.contains("offset")) {
                                    refClip.offset = refClipData["offset"].get<double>();
                                }
                                if (refClipData.contains("duration")) {
                                    refClip.duration = refClipData["duration"].get<double>();
                                }
                                if (refClipData.contains("volume")) {
                                    refClip.volume = refClipData["volume"].get<float>();
                                }
                                
                                track->setReferenceClip(refClip);
                            } else {
                                DEBUG_PRINT("Reference clip file not found: " + fileName);
                            }
                        }
                    }
                    
                    // Load clips
                    if (trackData.contains("clips") && trackData["clips"].is_array()) {
                        for (const auto& clipData : trackData["clips"]) {
                            if (clipData.contains("file")) {
                                std::string fileName = clipData["file"].get<std::string>();
                                juce::File file = findSampleFile(fileName);
                                
                                if (file.existsAsFile()) {
                                    AudioClip clip;
                                    clip.sourceFile = file;
                                    if (clipData.contains("startTime")) {
                                        clip.startTime = clipData["startTime"].get<double>();
                                    }
                                    if (clipData.contains("offset")) {
                                        clip.offset = clipData["offset"].get<double>();
                                    }
                                    if (clipData.contains("duration")) {
                                        clip.duration = clipData["duration"].get<double>();
                                    }
                                    if (clipData.contains("volume")) {
                                        clip.volume = clipData["volume"].get<float>();
                                    }
                                    
                                    track->addClip(clip);
                                } else {
                                    DEBUG_PRINT("Clip file not found: " + fileName);
                                }
                            }
                        }
                    }
                    
                    // Load track effects
                    if (trackData.contains("effects") && trackData["effects"].is_array()) {
                        track->clearEffects();
                        for (const auto& effectData : trackData["effects"]) {
                            if (effectData.contains("vstName")) {
                                std::string vstName = effectData["vstName"].get<std::string>();
                                
                                // Find VST file by name
                                juce::File vstFile = findVSTFile(vstName);
                                if (vstFile.existsAsFile()) {
                                    // Store effect for deferred loading to prevent blank window issues
                                    PendingEffect pendingEffect;
                                    pendingEffect.trackName = trackName;
                                    pendingEffect.vstPath = vstFile.getFullPathName().toStdString();
                                    
                                    if (effectData.contains("enabled")) {
                                        pendingEffect.enabled = effectData["enabled"].get<bool>();
                                    }
                                    if (effectData.contains("index")) {
                                        pendingEffect.index = effectData["index"].get<int>();
                                    }
                                    
                                    // Extract parameters
                                    if (effectData.contains("parameters") && effectData["parameters"].is_array()) {
                                        for (const auto& paramData : effectData["parameters"]) {
                                            if (paramData.contains("index") && paramData.contains("value")) {
                                                int paramIndex = paramData["index"].get<int>();
                                                float paramValue = paramData["value"].get<float>();
                                                pendingEffect.parameters.emplace_back(paramIndex, paramValue);
                                            }
                                        }
                                    }
                                    
                                    pendingEffects.push_back(pendingEffect);
                                    DEBUG_PRINT("Queued effect for deferred loading: " + vstName + " for track: " + trackName);
                                } else {
                                    DEBUG_PRINT("VST file not found for track effect: " + vstName);
                                }
                            }
                        }
                    }
                    
                    // Prepare track for playback and add to composition
                    track->prepareToPlay(sampleRate, currentBufferSize);
                    currentComposition->tracks.push_back(std::move(track));
                    DEBUG_PRINT("Track loaded and finalized: " + trackName);
                }
            }
        }
        
        // Prepare master track for playback
        if (masterTrack) {
            masterTrack->prepareToPlay(sampleRate, currentBufferSize);
        }
        
        currentState = stateData;
        DEBUG_PRINT("Engine state loaded successfully using nlohmann::json");
        DEBUG_PRINT("Composition: " + (currentComposition ? currentComposition->name : "null"));
        DEBUG_PRINT("Tracks loaded: " + std::to_string(currentComposition ? currentComposition->tracks.size() : 0));
        DEBUG_PRINT("Selected track: " + selectedTrackName);
        
    } catch (const json::parse_error& e) {
        DEBUG_PRINT("JSON parse error in loadState: " + std::string(e.what()));
    } catch (const json::exception& e) {
        DEBUG_PRINT("JSON error in loadState: " + std::string(e.what()));
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error in loadState: " + std::string(e.what()));
    }
}

void Engine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    sampleRate = device->getCurrentSampleRate();
    currentBufferSize = device->getCurrentBufferSizeSamples();
    tempMixBuffer.setSize(device->getOutputChannelNames().size(), currentBufferSize);
    tempMixBuffer.clear();
    positionSeconds = 0.0;
    
    DEBUG_PRINT("Engine: Device starting - sample rate: " << sampleRate << "Hz, buffer: " << currentBufferSize);
    
    // Prepare master track
    if (masterTrack) {
        masterTrack->prepareToPlay(sampleRate, currentBufferSize);
    }
    
    // Prepare all composition tracks
    if (currentComposition) {
        for (auto& track : currentComposition->tracks) {
            if (track) {
                track->prepareToPlay(sampleRate, currentBufferSize);
            }
        }
    }
    
    DBG("Device about to start with SR: " << sampleRate << ", buffer: " << currentBufferSize);
}

void Engine::audioDeviceStopped() {
    tempMixBuffer.setSize(0, 0);
}

void Engine::processBlock(juce::AudioBuffer<float>& outputBuffer, int numSamples) {
    outputBuffer.clear();
    tempMixBuffer.setSize(outputBuffer.getNumChannels(), numSamples, false, false, true);
    tempMixBuffer.clear();

    if (!currentComposition) return;

    bool anyTrackSoloed = false;
    for (const auto& track : currentComposition->tracks) {
        if (track && track->isSolo()) {
            anyTrackSoloed = true;
            break;
        }
    }

    for (const auto& track : currentComposition->tracks) {
        if (track) {
            bool shouldPlay = !anyTrackSoloed ? !track->isMuted() : track->isSolo();
            if (shouldPlay) {
                juce::AudioBuffer<float> trackBuffer;
                trackBuffer.setSize(tempMixBuffer.getNumChannels(), numSamples);
                trackBuffer.clear();
                
                track->process(positionSeconds, trackBuffer, numSamples, sampleRate);
                track->processEffects(trackBuffer);
                
                for (int ch = 0; ch < tempMixBuffer.getNumChannels(); ++ch) {
                    tempMixBuffer.addFrom(ch, 0, trackBuffer, ch, 0, numSamples);
                }
            }
        }
    }

    if (masterTrack && !masterTrack->isMuted()) {
        float masterGain = juce::Decibels::decibelsToGain(masterTrack->getVolume());
        float masterPan = masterTrack->getPan();

        float panL = (1.0f - juce::jlimit(-1.f, 1.f, masterPan)) * 0.5f;
        float panR = (1.0f + juce::jlimit(-1.f, 1.f, masterPan)) * 0.5f;
        
        if (tempMixBuffer.getNumChannels() >= 2) {
            tempMixBuffer.applyGain(0, 0, numSamples, masterGain * panL);
            tempMixBuffer.applyGain(1, 0, numSamples, masterGain * panR);
        } 
        else if (tempMixBuffer.getNumChannels() == 1) {
            tempMixBuffer.applyGain(0, 0, numSamples, masterGain);
        }

        masterTrack->processEffects(tempMixBuffer);

        outputBuffer.makeCopyOf(tempMixBuffer);
    }
}

std::vector<float> Engine::generateWaveformPeaks(const juce::File& audioFile, float duration, float peakResolution) {
    if (!audioFile.existsAsFile()) return {};
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
    if (!reader) return {};

    const long long totalSamples = reader->lengthInSamples;
    if (totalSamples == 0) return {};
    
    // Calculate peak generation parameters
    const int desiredPeaks = std::max(1, static_cast<int>(std::ceil(duration / peakResolution)));
    const long long samplesPerPeak = std::max(1LL, totalSamples / desiredPeaks);

    std::vector<float> peaks;
    peaks.reserve(desiredPeaks);

    // Buffer for reading audio data
    const int bufferSize = static_cast<int>(std::min(samplesPerPeak, 8192LL));
    juce::AudioBuffer<float> buffer(reader->numChannels, bufferSize);

    for (int i = 0; i < desiredPeaks; ++i) {
        const long long startSample = static_cast<long long>(i) * samplesPerPeak;
        if (startSample >= totalSamples) break;

        const int numSamplesToRead = static_cast<int>(
            std::min(static_cast<long long>(bufferSize), 
                    std::min(samplesPerPeak, totalSamples - startSample))
        );
        
        reader->read(&buffer, 0, numSamplesToRead, startSample, true, true);

        // Find peak in this segment
        float peak = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
            const float* channelData = buffer.getReadPointer(channel);
            for (int sample = 0; sample < numSamplesToRead; ++sample) {
                peak = std::max(peak, std::abs(channelData[sample]));
            }
        }
        peaks.push_back(peak);
    }
    
    return peaks;
}