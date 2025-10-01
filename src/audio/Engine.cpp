#include "Engine.hpp"
#include "AudioTrack.hpp"
#include "MIDITrack.hpp"
#include "Track.hpp"
#include "../DebugConfig.hpp"
#include <chrono>
#include <algorithm>
#include <cctype>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Engine::Engine() {
    formatManager.registerBasicFormats();
    playHead = std::make_unique<EnginePlayHead>();
    deviceManager.initialise(0, 2, nullptr, false);
    
    auto currentSetup = deviceManager.getAudioDeviceSetup();
    currentSetup.bufferSize = 256;
    juce::String error = deviceManager.setAudioDeviceSetup(currentSetup, true);
    
    if (error.isNotEmpty()) {
        currentSetup.bufferSize = 512;
        error = deviceManager.setAudioDeviceSetup(currentSetup, true);
        
        if (error.isNotEmpty()) {
            currentSetup.bufferSize = 1024;
            error = deviceManager.setAudioDeviceSetup(currentSetup, true);
        }
    }
    
    deviceManager.addAudioCallback(this);
    
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (const auto& deviceInfo : midiInputs) {
        deviceManager.setMidiInputDeviceEnabled(deviceInfo.identifier, true);
        deviceManager.addMidiInputDeviceCallback(deviceInfo.identifier, this);
    }

    masterTrack = std::make_unique<AudioTrack>(formatManager);
    masterTrack->setName("Master");
    selectedTrackName = "Master";

    juce::File exeFile = juce::File::getSpecialLocation(juce::File::SpecialLocationType::currentExecutableFile);
    juce::File exeDir = exeFile.getParentDirectory();
    while (!exeDir.isRoot() && !exeDir.getChildFile("assets").isDirectory()) {
        exeDir = exeDir.getParentDirectory();
    }
    juce::File soundsDir = exeDir.getChildFile("assets").getChildFile("sounds");
    metronomeDownbeatFile = soundsDir.getChildFile(metronomeDownbeatSample);
    metronomeUpbeatFile = soundsDir.getChildFile(metronomeUpbeatSample);
    
    // Initialize play head with default tempo (will be updated when composition is loaded)
    auto [timeSigNum, timeSigDen] = getTimeSignature();
    playHead->updatePosition(0.0, 120.0, false, sampleRate, timeSigNum, timeSigDen);
}

bool Engine::configureAudioDevice(double desiredSampleRate, int bufferSize) {
    DEBUG_PRINT("Configuring audio device: " << desiredSampleRate << " Hz, " << bufferSize << " samples");
    
    auto currentSetup = deviceManager.getAudioDeviceSetup();
    currentSetup.sampleRate = desiredSampleRate;
    currentSetup.bufferSize = bufferSize;
    
    juce::String error = deviceManager.setAudioDeviceSetup(currentSetup, true);
    
    if (error.isNotEmpty()) {
        std::vector<int> fallbackBuffers = {512, 1024, 2048};
        for (int fallbackBuffer : fallbackBuffers) {
            currentSetup.bufferSize = fallbackBuffer;
            error = deviceManager.setAudioDeviceSetup(currentSetup, true);
            
            if (error.isEmpty()) {
                break;
            }
        }
        
        if (error.isNotEmpty()) {
            return false;
        }
    }
    
    auto finalSetup = deviceManager.getAudioDeviceSetup();
    sampleRate = finalSetup.sampleRate;
    currentBufferSize = finalSetup.bufferSize;
    
    return true;
}

Engine::~Engine() {
    auto midiInputs = juce::MidiInput::getAvailableDevices();
    for (const auto& deviceInfo : midiInputs) {
        deviceManager.removeMidiInputDeviceCallback(deviceInfo.identifier, this);
        deviceManager.setMidiInputDeviceEnabled(deviceInfo.identifier, false);
    }
    
    deviceManager.closeAudioDevice();
    deviceManager.removeAudioCallback(this);
}

void Engine::setMetronomeEnabled(bool enabled) {
    metronomeEnabled = enabled;
}

void Engine::generateMetronomeTrack() {
    if (!currentComposition) {
        currentComposition = std::make_unique<Composition>();
    }
    metronomeTrack = std::make_unique<AudioTrack>(formatManager);
    metronomeTrack->setName("__metronome__");
    metronomeTrack->prepareToPlay(sampleRate, currentBufferSize);
    metronomeTrack->clearClips();

    double bpm = getBpm();
    auto [num, den] = getTimeSignature();
    double beatLength = 60.0 / bpm;
    double barLength = beatLength * num;
    double projectLength = 0.0;
    bool hasClips = false;
    for (const auto& track : currentComposition->tracks) {
        for (const auto& clip : track->getClips()) {
            hasClips = true;
            double end = clip.startTime + clip.duration;
            if (end > projectLength) projectLength = end;
        }
    }
    // If no clips, set a default project length (e.g., 4 bars)
    if (!hasClips) {
        projectLength = 4 * barLength;
    }
    int numBars = static_cast<int>(std::ceil(projectLength / barLength));
    for (int bar = 0; bar < numBars; ++bar) {
        double barStart = bar * barLength;
        for (int beat = 0; beat < num; ++beat) {
            double beatTime = barStart + beat * beatLength;
            const juce::File& sampleFile = (beat == 0) ? metronomeDownbeatFile : metronomeUpbeatFile;
            if (sampleFile.existsAsFile()) {
                double lengthSeconds = 0.1;
                if (auto* reader = formatManager.createReaderFor(sampleFile)) {
                    lengthSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
                    delete reader;
                }
                metronomeTrack->addClip({sampleFile, beatTime, 0.0, lengthSeconds, 1.0f});
            }
        }
    }
}

void Engine::exportMaster(const std::string& filePath) {
    double startTime = std::numeric_limits<double>::max();
    double endTime = 0.0;

    bool hasClips = false;
    for (const auto& track : currentComposition->tracks) {
        for (const auto& clip : track->getClips()) {
            hasClips = true;
            if (clip.startTime < startTime) startTime = clip.startTime;
            double clipEnd = clip.startTime + clip.duration;
            if (clipEnd > endTime) endTime = clipEnd;
        }
    }
    // If no clips, set a default export length (e.g., 4 bars)
    if (!hasClips) {
        startTime = 0.0;
        double bpm = getBpm();
        auto [num, den] = getTimeSignature();
        double beatLength = 60.0 / bpm;
        double barLength = beatLength * num;
        endTime = 4 * barLength;
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
        juce::AudioBuffer<float> tempMixBuffer(numChannels, samplesToProcess);
        tempMixBuffer.clear();

        // Mix main composition if playing (for export, always mix)
        if (currentComposition) {
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
                        juce::AudioBuffer<float> trackBuffer(numChannels, samplesToProcess);
                        trackBuffer.clear();
                        track->process(positionSeconds, trackBuffer, samplesToProcess, sampleRate);
                        track->processEffects(trackBuffer);
                        for (int ch = 0; ch < numChannels; ++ch) {
                            tempMixBuffer.addFrom(ch, 0, trackBuffer, ch, 0, samplesToProcess);
                        }
                    }
                }
            }
        }

        // No preview/one-shot for export

        // Apply master effects and gain
        if (masterTrack && !masterTrack->isMuted()) {
            masterTrack->processEffects(tempMixBuffer);
            float masterGain = juce::Decibels::decibelsToGain(masterTrack->getVolume());
            float masterPan = masterTrack->getPan();
            float panL = std::cos((masterPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            float panR = std::sin((masterPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
            if (numChannels >= 2) {
                tempMixBuffer.applyGain(0, 0, samplesToProcess, masterGain * panL);
                tempMixBuffer.applyGain(1, 0, samplesToProcess, masterGain * panR);
                for (int ch = 2; ch < numChannels; ++ch)
                    tempMixBuffer.applyGain(ch, 0, samplesToProcess, masterGain);
            } else if (numChannels == 1) {
                tempMixBuffer.applyGain(0, 0, samplesToProcess, masterGain);
            }
        }

        // Copy to output
        for (int ch = 0; ch < numChannels; ++ch)
            outputBuffer.copyFrom(ch, pos, tempMixBuffer, ch, 0, samplesToProcess);
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
}

void Engine::setSampleDirectory(const std::string& directory) {
    sampleDirectory = directory;
}

std::string Engine::getVSTDirectory() const {
    return vstDirectory;
}

std::string Engine::getSampleDirectory() const {
    return sampleDirectory;
}

juce::File Engine::findSampleFile(const std::string& sampleName) const {
    if (sampleName.empty()) {
        return juce::File();
    }

    // 1. Try user sample directory if set
    if (!sampleDirectory.empty()) {
        juce::File directory(sampleDirectory);
        if (directory.exists() && directory.isDirectory()) {
            // Exact filename match
            juce::File exactFile = directory.getChildFile(sampleName);
            if (exactFile.existsAsFile()) {
                return exactFile;
            }
            // Try common extensions
            std::vector<std::string> extensions = {".wav", ".mp3", ".flac", ".ogg", ".aiff", ".m4a"};
            for (const auto& ext : extensions) {
                juce::File file = directory.getChildFile(sampleName + ext);
                if (file.existsAsFile()) {
                    return file;
                }
            }
            // Case-insensitive search
            juce::Array<juce::File> allFiles;
            directory.findChildFiles(allFiles, juce::File::findFiles, false);
            for (const auto& file : allFiles) {
                std::string fileName = file.getFileName().toStdString();
                std::string targetName = sampleName;
                std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
                std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
                if (fileName == targetName) {
                    return file;
                }
            }
        }
    }

    // 2. Try assets/sounds folder in executable directory only
    juce::File exeFile = juce::File::getSpecialLocation(juce::File::SpecialLocationType::currentExecutableFile);
    // Always use the top-level executable directory, not any subfolder
    juce::File exeDir = exeFile.getParentDirectory();
    // If the exe is inside a subfolder (like extensions), go up until we find the directory containing 'assets'
    while (!exeDir.isRoot() && !exeDir.getChildFile("assets").isDirectory()) {
        exeDir = exeDir.getParentDirectory();
    }
    juce::File soundsDir = exeDir.getChildFile("assets").getChildFile("sounds");
    if (soundsDir.exists() && soundsDir.isDirectory()) {
        // Exact filename match
        juce::File exactFile = soundsDir.getChildFile(sampleName);
        if (exactFile.existsAsFile()) {
            return exactFile;
        }
        // Try common extensions
        std::vector<std::string> extensions = {".wav", ".mp3", ".flac", ".ogg", ".aiff", ".m4a"};
        for (const auto& ext : extensions) {
            juce::File file = soundsDir.getChildFile(sampleName + ext);
            if (file.existsAsFile()) {
                return file;
            }
        }
        // Case-insensitive search
        juce::Array<juce::File> allFiles;
        soundsDir.findChildFiles(allFiles, juce::File::findFiles, false);
        for (const auto& file : allFiles) {
            std::string fileName = file.getFileName().toStdString();
            std::string targetName = sampleName;
            std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
            std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
            if (fileName == targetName) {
                return file;
            }
        }
    }

    return juce::File();
}

juce::File Engine::findVSTFile(const std::string& vstName) const {
    if (vstDirectory.empty() || vstName.empty()) {
        return juce::File();
    }
    
    juce::File directory(vstDirectory);
    if (!directory.exists() || !directory.isDirectory()) {
        return juce::File();
    }
    
    // First try exact filename match (with extension)
    juce::File exactFile = directory.getChildFile(vstName);
    if (exactFile.existsAsFile()) {
        return exactFile;
    }
    
    // If no exact match, try adding common VST file extensions
    std::vector<std::string> extensions = {".dll", ".vst", ".vst3"};
    
    for (const auto& ext : extensions) {
        juce::File file = directory.getChildFile(vstName + ext);
        if (file.existsAsFile()) {
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
            return file;
        }
    }
    
    return juce::File();
}

void Engine::play() {
    if (hasSaved) {
        positionSeconds = savedPosition;
        hasSaved = false;
    }
    if (metronomeEnabled) {
        generateMetronomeTrack();
    }
    
    // Clear all synthesizer buffers when playback starts
    // This stops reverb tails, delay feedback, and other sustained effects
    if (currentComposition) {
        DEBUG_PRINT("Clearing synthesizer buffers on playback start...");
        
        // Set countdown to keep synthesizers silenced for a few audio cycles
        // BUT: Don't silence if there are MIDI events at the very start (relative to playhead)
        bool hasMidiAtStart = false;
        double currentPlayheadTime = positionSeconds;
        for (const auto& track : currentComposition->tracks) {
            if (track && track->getType() == Track::TrackType::MIDI) {
                auto midiTrack = dynamic_cast<MIDITrack*>(track.get());
                if (midiTrack) {
                    // Check if any MIDI clips have events near the current playhead
                    for (const auto& clip : midiTrack->getMIDIClips()) {
                        // Check if clip overlaps with the playhead + 0.1 second window
                        if (clip.startTime <= currentPlayheadTime + 0.1 && clip.getEndTime() >= currentPlayheadTime) {
                            // Check if this clip has MIDI events at its beginning relative to playhead
                            for (const auto& event : clip.midiData) {
                                double eventTimeInClip = event.samplePosition / 44100.0;
                                double absoluteEventTime = clip.startTime + eventTimeInClip;
                                // Check if event happens within 0.1 seconds of current playhead
                                if (absoluteEventTime >= currentPlayheadTime && absoluteEventTime <= currentPlayheadTime + 0.1) {
                                    hasMidiAtStart = true;
                                    break;
                                }
                            }
                            if (hasMidiAtStart) break;
                        }
                    }
                }
            }
            if (hasMidiAtStart) break;
        }
        
        // Use reduced silence countdown if MIDI starts immediately, otherwise use full countdown
        synthSilenceCountdown = hasMidiAtStart ? 1 : SYNTH_SILENCE_CYCLES;
        if (synthSilenceCountdown > 1) {
            DEBUG_PRINT("Silencing synthesizers for " << SYNTH_SILENCE_CYCLES << " cycles (no MIDI at start)");
        } else {
            DEBUG_PRINT("Reduced synthesizer silence to 1 cycle (MIDI detected at start)");
        }
        
        // Send current BPM to all synthesizers before clearing buffers
        // This ensures they have the correct tempo for any LFOs or tempo-synced effects
        sendBpmToSynthesizers();
        
        for (const auto& track : currentComposition->tracks) {
            if (track && track->getType() == Track::TrackType::MIDI) {
                auto midiTrack = dynamic_cast<MIDITrack*>(track.get());
                if (midiTrack) {
                    for (const auto& effect : midiTrack->getEffects()) {
                        if (effect && effect->enabled() && effect->isSynthesizer()) {
                            DEBUG_PRINT("Resetting buffers for synthesizer: " << effect->getName());
                            effect->resetBuffers();
                        }
                    }
                }
            }
        }
        
        // Also clear master track effects if they're synthesizers (unlikely but possible)
        if (masterTrack) {
            for (const auto& effect : masterTrack->getEffects()) {
                if (effect && effect->enabled() && effect->isSynthesizer()) {
                    DEBUG_PRINT("Resetting buffers for master synthesizer: " << effect->getName());
                    effect->resetBuffers();
                }
            }
        }
        
        DEBUG_PRINT("Synthesizer buffer clearing complete. Silencing for " << SYNTH_SILENCE_CYCLES << " audio cycles.");
    }
    
    playing = true;
}

void Engine::pause() {
    playing = false;
    
    // Send "All Notes Off" to prevent stuck notes when pausing
    if (currentComposition) {
        for (const auto& track : currentComposition->tracks) {
            if (auto midiTrack = dynamic_cast<MIDITrack*>(track.get())) {
                midiTrack->sendAllNotesOff();
            }
        }
    }
}

void Engine::stop() {
    playing = false;
    positionSeconds = 0.0;
    
    // Send "All Notes Off" (MIDI CC 123) to all synthesizers to prevent stuck notes
    if (currentComposition) {
        for (const auto& track : currentComposition->tracks) {
            if (track && track->getType() == Track::TrackType::MIDI) {
                MIDITrack* midiTrack = dynamic_cast<MIDITrack*>(track.get());
                if (midiTrack) {
                    midiTrack->sendAllNotesOff();
                }
            }
        }
    }
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
    stop();
    currentComposition = std::make_unique<Composition>();
    currentComposition->name = name;
    
    DEBUG_PRINT("Created new composition '" << name << "'");
    
    // Send BPM to any existing synthesizers (in case user has loaded synths before creating composition)
    sendBpmToSynthesizers();
}

void Engine::loadComposition(const std::string& path) {
    stop();
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
    if (metronomeEnabled) {
        generateMetronomeTrack();
    }
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
        if (metronomeEnabled) {
            generateMetronomeTrack();
        }
        
        sendBpmToSynthesizers();
    }
}

void Engine::sendBpmToSynthesizers() {
    if (!currentComposition) {
        return;
    }
    
    double currentBpm = getBpm();
    
    // Update play head with current tempo even when not playing
    double sampleRate = getSampleRate();
    auto [timeSigNum, timeSigDen] = getTimeSignature();
    playHead->updatePosition(positionSeconds, currentBpm, playing, sampleRate, timeSigNum, timeSigDen);
    
    for (const auto& track : currentComposition->tracks) {
        if (track && track->getType() == Track::TrackType::MIDI) {
            auto midiTrack = dynamic_cast<MIDITrack*>(track.get());
            if (midiTrack) {
                // Send BPM to all effects that are synthesizers
                for (const auto& effect : midiTrack->getEffects()) {
                    if (effect && effect->enabled() && effect->isSynthesizer()) {
                        effect->setBpm(currentBpm);
                        effect->setPlayHead(playHead.get());  // Connect our custom playhead
                    }
                }
            }
        }
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

    auto t = std::make_unique<AudioTrack>(formatManager);
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

std::string Engine::addMIDITrack(const std::string& name) {
    if (!currentComposition) {
        currentComposition = std::make_unique<Composition>();
        currentComposition->name = "untitled";
    }
    
    std::string baseName = name.empty() ? "MIDI Track" : name;
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

    auto midiTrack = std::make_unique<MIDITrack>();
    midiTrack->setName(uniqueName);
    midiTrack->prepareToPlay(sampleRate, currentBufferSize);
    
    currentComposition->tracks.push_back(std::move(midiTrack));
    
    DEBUG_PRINT("Added MIDI track '" << uniqueName << "'");
    return uniqueName;
}

void Engine::removeTrack(int idx) {
    if (currentComposition && idx >= 0 && idx < currentComposition->tracks.size()) {
        currentComposition->tracks.erase(currentComposition->tracks.begin() + idx);
    }
}

void Engine::removeTrackByName(const std::string& name) {
    if (currentComposition) {
        DEBUG_PRINT("Removing track: " << name);
        for (int i = 0; i < currentComposition->tracks.size(); i++) {
            if (currentComposition->tracks[i]->getName() == name) {
                auto& track = currentComposition->tracks[i];
                currentComposition->tracks.erase(currentComposition->tracks.begin() + i);
                break;
            }
        }
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
    if (tempMixBuffer.getNumChannels() != numOutputChannels || tempMixBuffer.getNumSamples() != numSamples) {
        tempMixBuffer.setSize(numOutputChannels, numSamples, false, false, true);
    }
    tempMixBuffer.clear();

    // Manage synthesizer silence countdown after playback starts
    if (synthSilenceCountdown > 0) {
        synthSilenceCountdown--;
        if (synthSilenceCountdown == 0) {
            // Re-enable all synthesizers after silence period
            DEBUG_PRINT("Re-enabling synthesizers after silence period");
            if (currentComposition) {
                for (const auto& track : currentComposition->tracks) {
                    if (track && track->getType() == Track::TrackType::MIDI) {
                        auto midiTrack = dynamic_cast<MIDITrack*>(track.get());
                        if (midiTrack) {
                            for (const auto& effect : midiTrack->getEffects()) {
                                if (effect && effect->enabled() && effect->isSynthesizer()) {
                                    effect->setSilenced(false);
                                }
                            }
                        }
                    }
                }
                // Also handle master track synthesizers
                if (masterTrack) {
                    for (const auto& effect : masterTrack->getEffects()) {
                        if (effect && effect->enabled() && effect->isSynthesizer()) {
                            effect->setSilenced(false);
                        }
                    }
                }
            }
        }
    }

    // 0. Process real-time MIDI input (works even when paused)
    bool processedRealtimeMidi = false;
    {
        juce::ScopedLock lock(midiInputLock);
        if (!incomingMidiBuffer.isEmpty()) {
            processedRealtimeMidi = true;
            DEBUG_PRINT("Processing real-time MIDI messages");
            
            // Update AudioPlayHead with current tempo for real-time MIDI processing
            if (playHead && currentComposition) {
                double currentBpm = currentComposition->bpm;
                auto [timeSigNum, timeSigDen] = getTimeSignature();
                playHead->updatePosition(positionSeconds, currentBpm, false, sampleRate, timeSigNum, timeSigDen);
            }
            
            auto selectedTrack = getSelectedTrackPtr();
            if (selectedTrack) {
                auto midiTrack = dynamic_cast<MIDITrack*>(selectedTrack);
                if (midiTrack) {
                    DEBUG_PRINT("Sending MIDI to track: " << midiTrack->getName());
                    // Process real-time MIDI through the selected MIDI track
                    juce::AudioBuffer<float> realtimeBuffer(numOutputChannels, numSamples);
                    realtimeBuffer.clear();
                    
                    // Process MIDI directly without any processing modifications
                    for (const auto& effect : midiTrack->getEffects()) {
                        if (effect && effect->enabled() && effect->isSynthesizer()) {
                            effect->processAudio(realtimeBuffer, incomingMidiBuffer);
                        }
                    }
                    
                    // Mix the real-time MIDI output into the main buffer
                    for (int ch = 0; ch < numOutputChannels; ++ch) {
                        tempMixBuffer.addFrom(ch, 0, realtimeBuffer, ch, 0, numSamples);
                    }
                } else {
                    DEBUG_PRINT("Selected track is not a MIDI track");
                }
            } else {
                DEBUG_PRINT("No track selected for real-time MIDI");
            }
            incomingMidiBuffer.clear(); // Clear processed MIDI messages
        }
    }

    // 0.5. Process synthesizers continuously to maintain audio (only when NOT playing to avoid interference)
    if (!playing && currentComposition) {
        // Update play head even when not playing so synthesizers have correct tempo
        double currentBpm = getBpm();
        double sampleRate = getSampleRate();
        auto [timeSigNum, timeSigDen] = getTimeSignature();
        playHead->updatePosition(positionSeconds, currentBpm, playing, sampleRate, timeSigNum, timeSigDen);
        
        juce::AudioBuffer<float> synthBuffer(numOutputChannels, numSamples);
        juce::MidiBuffer emptyMidiBuffer; // Empty MIDI buffer for continuous processing
        
        // Get selected track to avoid double processing
        auto selectedTrack = getSelectedTrackPtr();
        
        for (const auto& track : currentComposition->tracks) {
            if (track && track->getType() == Track::TrackType::MIDI) {
                auto midiTrack = dynamic_cast<MIDITrack*>(track.get());
                if (midiTrack && (!midiTrack->isMuted())) {
                    
                    // Skip the selected track if we just processed real-time MIDI for it
                    // This prevents double processing and audio conflicts
                    if (processedRealtimeMidi && track.get() == selectedTrack) {
                        DEBUG_PRINT("Skipping continuous synthesis for selected track to avoid conflicts");
                        continue;
                    }
                    
                    synthBuffer.clear();
                    
                    // Process synthesizers with empty MIDI to maintain sustain/envelope states
                    for (const auto& effect : midiTrack->getEffects()) {
                        if (effect && effect->enabled() && effect->isSynthesizer()) {
                            effect->processAudio(synthBuffer, emptyMidiBuffer);
                        }
                    }
                    
                    // Apply track volume and mix into main buffer
                    float trackGain = juce::Decibels::decibelsToGain(midiTrack->getVolume());
                    for (int ch = 0; ch < numOutputChannels; ++ch) {
                        tempMixBuffer.addFrom(ch, 0, synthBuffer, ch, 0, numSamples, trackGain);
                    }
                }
            }
        }
    }

    // 1. Process main composition if the transport is playing.
    if (playing && currentComposition)
    {
        // Update play head with current position and tempo
        double currentBpm = getBpm();
        double sampleRate = getSampleRate();
        auto [timeSigNum, timeSigDen] = getTimeSignature();
        playHead->updatePosition(positionSeconds, currentBpm, playing, sampleRate, timeSigNum, timeSigDen);
        
        juce::AudioBuffer<float> trackBuffer(numOutputChannels, numSamples);
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
                    trackBuffer.clear();
                    track->applyAutomation(positionSeconds);
                    track->process(positionSeconds, trackBuffer, numSamples, sampleRate);
                    
                    // For MIDI tracks, effects are processed internally with MIDI data
                    // For Audio tracks, we need to process effects separately
                    if (track->getType() != Track::TrackType::MIDI) {
                        track->processEffects(trackBuffer);
                    }
                    
                    // Check for audio output from this track
                    float trackPeak = 0.0f;
                    for (int ch = 0; ch < numOutputChannels; ++ch) {
                        for (int i = 0; i < numSamples; ++i) {
                            trackPeak = std::max(trackPeak, std::abs(trackBuffer.getSample(ch, i)));
                        }
                    }
                    if (trackPeak > 0.001f) { // Only log if there's significant audio
                        DEBUG_PRINT("Track '" << track->getName() << "' peak: " << trackPeak);
                    }
                    
                    for (int ch = 0; ch < numOutputChannels; ++ch) {
                        tempMixBuffer.addFrom(ch, 0, trackBuffer, ch, 0, numSamples);
                    }
                }
            }
        }
        // --- Mix metronome track if enabled ---
        if (metronomeEnabled && metronomeTrack) {
            trackBuffer.clear();
            metronomeTrack->process(positionSeconds, trackBuffer, numSamples, sampleRate);
            for (int ch = 0; ch < numOutputChannels; ++ch) {
                tempMixBuffer.addFrom(ch, 0, trackBuffer, ch, 0, numSamples);
            }
        }
        positionSeconds += static_cast<double>(numSamples) / sampleRate;
    }

    // 2. Process and mix the one-shot preview sound (ALWAYS).
    if (previewSource && previewTransport.isPlaying())
    {
        juce::AudioBuffer<float> previewBuffer(numOutputChannels, numSamples); // stack-allocated
        juce::AudioSourceChannelInfo previewInfo(&previewBuffer, 0, numSamples);
        previewTransport.getNextAudioBlock(previewInfo);
        for (int ch = 0; ch < numOutputChannels; ++ch) {
            tempMixBuffer.addFrom(ch, 0, previewBuffer, ch, 0, numSamples);
        }
    }

    // 3. Apply master track effects and gain to the final mix.
    if (masterTrack && !masterTrack->isMuted())
    {
        masterTrack->processEffects(tempMixBuffer);
        float masterGain = juce::Decibels::decibelsToGain(masterTrack->getVolume());
        float masterPan = masterTrack->getPan();
        float panL = std::cos((masterPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        float panR = std::sin((masterPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        if (numOutputChannels >= 2) {
            tempMixBuffer.applyGain(0, 0, numSamples, masterGain * panL);
            tempMixBuffer.applyGain(1, 0, numSamples, masterGain * panR);
            for (int ch = 2; ch < numOutputChannels; ++ch)
                tempMixBuffer.applyGain(ch, 0, numSamples, masterGain);
        } else if (numOutputChannels == 1) {
            tempMixBuffer.applyGain(0, 0, numSamples, masterGain);
        }
    }

    // 4. Finally, copy our processed audio to the hardware output.
    juce::AudioBuffer<float> out(outputChannelData, numOutputChannels, numSamples);
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        out.copyFrom(ch, 0, tempMixBuffer, ch, 0, numSamples);
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
                    masterTrack = std::make_unique<AudioTrack>(formatManager);
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
                    auto track = std::make_unique<AudioTrack>(formatManager);
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
        
        // Send BPM to all loaded synthesizers after project load
        sendBpmToSynthesizers();
        
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

void Engine::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) {
    // Only process MIDI note messages (note on/off)
    if (!message.isNoteOnOrOff()) return;
    
    DEBUG_PRINT("MIDI Input: " << message.getDescription().toStdString());
    
    // Queue the MIDI message for processing in the audio thread
    juce::ScopedLock lock(midiInputLock);
    incomingMidiBuffer.addEvent(message, 0);
}

void Engine::sendRealtimeMIDI(int noteNumber, int velocity, bool noteOn) {
    // Create the MIDI message
    juce::MidiMessage message;
    if (noteOn) {
        message = juce::MidiMessage::noteOn(1, noteNumber, static_cast<juce::uint8>(velocity));
        DEBUG_PRINT("Queuing realtime MIDI Note On: " << noteNumber << " velocity: " << velocity);
    } else {
        // Use a moderate release velocity (64) for smoother note-off
        message = juce::MidiMessage::noteOff(1, noteNumber, static_cast<juce::uint8>(64));
        DEBUG_PRINT("Queuing realtime MIDI Note Off: " << noteNumber);
    }
    
    // Queue the message for processing in the audio thread (no direct processing)
    {
        juce::ScopedLock lock(midiInputLock);
        incomingMidiBuffer.addEvent(message, 0);
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

void Engine::playSound(const std::string& filePath, float volume) {
    juce::File file(filePath);
    if (!file.existsAsFile()) {
        return;
    }

    playSound(file, volume);
}

void Engine::playSound(const juce::File& file, float volume) {
    const juce::ScopedLock lock (deviceManager.getAudioCallbackLock());

    previewTransport.stop();
    previewTransport.setSource(nullptr);
    previewSource.reset();

    if (!file.existsAsFile()) {
        return;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) {
        return;
    }

    previewSource = std::make_unique<juce::AudioFormatReaderSource>(reader.release(), true);
    previewTransport.prepareToPlay(currentBufferSize, sampleRate);
    previewTransport.setSource(previewSource.get(), 0, nullptr, sampleRate);
    previewTransport.setGain(juce::jlimit(0.0f, 2.0f, volume));
    previewTransport.setPosition(0.0);
    previewTransport.start();
}
