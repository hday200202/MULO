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
    
    lastStateChangeTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch());
    
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
    int numChannels = 2;
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

    juce::String basePath = juce::String(filePath);
    if (!basePath.endsWithChar('/') && !basePath.endsWithChar('\\'))
        basePath += juce::File::getSeparatorString();
    juce::String fileName = currentComposition->name;
    if (!fileName.endsWithIgnoreCase(".wav"))
        fileName += ".wav";
    juce::File outFile(basePath + fileName);
    outFile = outFile.getNonexistentSibling();
    outFile.getParentDirectory().createDirectory();

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

void Engine::markStateChanged() {
    lastStateChangeTimestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch());
    
    // Update state hash for efficient change detection
    lastStateHash = getStateHash();
}

std::string Engine::getStateHash() const {
    if (!currentComposition) {
        return "empty";
    }
    
    std::string hashSource = currentComposition->name + 
                           std::to_string(currentComposition->bpm) + 
                           std::to_string(currentComposition->tracks.size());
    
    for (const auto& track : currentComposition->tracks) {
        if (track) {
            hashSource += track->getName();
            hashSource += std::to_string(static_cast<int>(track->getType()));
            
            const auto& clips = track->getClips();
            hashSource += std::to_string(clips.size());
            for (const auto& clip : clips) {
                hashSource += clip.sourceFile.getFileName().toStdString();
                hashSource += std::to_string(clip.startTime);
                hashSource += std::to_string(clip.duration);
            }
            
            if (track->getType() == Track::TrackType::MIDI) {
                auto* midiTrack = dynamic_cast<MIDITrack*>(track.get());
                if (midiTrack) {
                    const auto& midiClips = midiTrack->getMIDIClips();
                    hashSource += std::to_string(midiClips.size());
                    for (const auto& clip : midiClips) {
                        hashSource += std::to_string(clip.startTime);
                        hashSource += std::to_string(clip.duration);
                        hashSource += std::to_string(clip.midiData.getNumEvents());
                    }
                }
            }
        }
    }
    
    // Simple hash (for collaboration change detection, not cryptographic security)
    std::hash<std::string> hasher;
    return std::to_string(hasher(hashSource));
}

juce::File Engine::findSampleFile(const std::string& sampleName) const {
    DEBUG_PRINT("findSampleFile called with: '" + sampleName + "'");
    
    if (sampleName.empty()) {
        DEBUG_PRINT("Empty sample name, returning empty file");
        return juce::File();
    }

    if (!sampleDirectory.empty()) {
        juce::File directory(sampleDirectory);
        DEBUG_PRINT("Checking user directory: " + directory.getFullPathName().toStdString());
        if (directory.exists() && directory.isDirectory()) {
            DEBUG_PRINT("User directory exists and is valid");
            // First try quick exact match in root directory
            juce::File exactFile = directory.getChildFile(sampleName);
            if (exactFile.existsAsFile()) {
                DEBUG_PRINT("Found exact file in user root directory: " + exactFile.getFullPathName().toStdString());
                return exactFile;
            }
            
            // Quick extension tries in root directory
            std::vector<std::string> extensions = {".wav", ".mp3", ".flac", ".ogg", ".aiff", ".m4a"};
            for (const auto& ext : extensions) {
                juce::File file = directory.getChildFile(sampleName + ext);
                if (file.existsAsFile()) {
                    return file;
                }
            }
            
            juce::Array<juce::File> allFiles;
            directory.findChildFiles(allFiles, juce::File::findFiles, true);
            DEBUG_PRINT("Found " + std::to_string(allFiles.size()) + " files in user directory recursively");
            
            for (const auto& file : allFiles) {
                if (file.getFileName().toStdString() == sampleName) {
                    DEBUG_PRINT("Found exact match: " + file.getFullPathName().toStdString());
                    return file;
                }
            }
            
            for (const auto& ext : extensions) {
                std::string targetWithExt = sampleName + ext;
                for (const auto& file : allFiles) {
                    if (file.getFileName().toStdString() == targetWithExt) {
                        DEBUG_PRINT("Found file with extension: " + file.getFullPathName().toStdString());
                        return file;
                    }
                }
            }
            
            for (const auto& file : allFiles) {
                std::string fileName = file.getFileName().toStdString();
                std::string targetName = sampleName;
                std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
                std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
                if (fileName == targetName) {
                    DEBUG_PRINT("Found case-insensitive match: " + file.getFullPathName().toStdString());
                    return file;
                }
                
                for (const auto& ext : extensions) {
                    std::string targetWithExt = targetName + ext;
                    if (fileName == targetWithExt) {
                        DEBUG_PRINT("Found case-insensitive match with extension: " + file.getFullPathName().toStdString());
                        return file;
                    }
                }
            }
            DEBUG_PRINT("No match found in user directory");
        } else {
            DEBUG_PRINT("User directory does not exist or is not a directory");
        }
    } else {
        DEBUG_PRINT("No user sample directory set");
    }

    DEBUG_PRINT("Searching in assets/sounds directory");
    juce::File exeFile = juce::File::getSpecialLocation(juce::File::SpecialLocationType::currentExecutableFile);
    juce::File exeDir = exeFile.getParentDirectory();
    while (!exeDir.isRoot() && !exeDir.getChildFile("assets").isDirectory()) {
        exeDir = exeDir.getParentDirectory();
    }
    juce::File soundsDir = exeDir.getChildFile("assets").getChildFile("sounds");
    DEBUG_PRINT("Assets sounds directory: " + soundsDir.getFullPathName().toStdString());
    if (soundsDir.exists() && soundsDir.isDirectory()) {
        DEBUG_PRINT("Assets sounds directory exists and is valid");
        // Quick exact match in root assets/sounds directory
        juce::File exactFile = soundsDir.getChildFile(sampleName);
        if (exactFile.existsAsFile()) {
            DEBUG_PRINT("Found exact file in assets root: " + exactFile.getFullPathName().toStdString());
            return exactFile;
        }
        
        // Quick extension tries in root assets/sounds directory
        std::vector<std::string> extensions = {".wav", ".mp3", ".flac", ".ogg", ".aiff", ".m4a"};
        for (const auto& ext : extensions) {
            juce::File file = soundsDir.getChildFile(sampleName + ext);
            if (file.existsAsFile()) {
                return file;
            }
        }
        
        // Comprehensive recursive search
        juce::Array<juce::File> allFiles;
        soundsDir.findChildFiles(allFiles, juce::File::findFiles, true); // true = recursive search
        DEBUG_PRINT("Found " + std::to_string(allFiles.size()) + " files in assets/sounds recursively");
        
        // First pass: exact filename match
        for (const auto& file : allFiles) {
            if (file.getFileName().toStdString() == sampleName) {
                DEBUG_PRINT("Found exact match in assets: " + file.getFullPathName().toStdString());
                return file;
            }
        }
        
        // Second pass: filename with extensions
        for (const auto& ext : extensions) {
            std::string targetWithExt = sampleName + ext;
            for (const auto& file : allFiles) {
                if (file.getFileName().toStdString() == targetWithExt) {
                    DEBUG_PRINT("Found file with extension in assets: " + file.getFullPathName().toStdString());
                    return file;
                }
            }
        }
        
        // Third pass: case-insensitive search
        for (const auto& file : allFiles) {
            std::string fileName = file.getFileName().toStdString();
            std::string targetName = sampleName;
            std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
            std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
            if (fileName == targetName) {
                DEBUG_PRINT("Found case-insensitive match in assets: " + file.getFullPathName().toStdString());
                return file;
            }
            
            // Also check with extensions in case-insensitive way
            for (const auto& ext : extensions) {
                std::string targetWithExt = targetName + ext;
                if (fileName == targetWithExt) {
                    DEBUG_PRINT("Found case-insensitive match with extension in assets: " + file.getFullPathName().toStdString());
                    return file;
                }
            }
        }
        DEBUG_PRINT("No match found in assets/sounds");
    } else {
        DEBUG_PRINT("Assets sounds directory does not exist or is not a directory");
    }

    // 3. Try assets/test_samples folder as fallback for project samples
    DEBUG_PRINT("Searching in assets/test_samples directory");
    juce::File testSamplesDir = exeDir.getChildFile("assets").getChildFile("test_samples");
    DEBUG_PRINT("Test samples directory: " + testSamplesDir.getFullPathName().toStdString());
    if (testSamplesDir.exists() && testSamplesDir.isDirectory()) {
        DEBUG_PRINT("Test samples directory exists and is valid");
        // Quick exact match in test_samples directory
        juce::File exactFile = testSamplesDir.getChildFile(sampleName);
        if (exactFile.existsAsFile()) {
            DEBUG_PRINT("Found exact file in test_samples: " + exactFile.getFullPathName().toStdString());
            return exactFile;
        }
        
        // Quick extension tries in test_samples directory
        std::vector<std::string> extensions = {".wav", ".mp3", ".flac", ".ogg", ".aiff", ".m4a"};
        for (const auto& ext : extensions) {
            juce::File file = testSamplesDir.getChildFile(sampleName + ext);
            if (file.existsAsFile()) {
                DEBUG_PRINT("Found file with extension in test_samples: " + file.getFullPathName().toStdString());
                return file;
            }
        }
        
        // Comprehensive recursive search
        juce::Array<juce::File> allFiles;
        testSamplesDir.findChildFiles(allFiles, juce::File::findFiles, true); // true = recursive search
        DEBUG_PRINT("Found " + std::to_string(allFiles.size()) + " files in test_samples recursively");
        
        // First pass: exact filename match
        for (const auto& file : allFiles) {
            if (file.getFileName().toStdString() == sampleName) {
                DEBUG_PRINT("Found exact match in test_samples: " + file.getFullPathName().toStdString());
                return file;
            }
        }
        
        // Second pass: filename with extensions
        for (const auto& ext : extensions) {
            std::string targetWithExt = sampleName + ext;
            for (const auto& file : allFiles) {
                if (file.getFileName().toStdString() == targetWithExt) {
                    DEBUG_PRINT("Found file with extension in test_samples: " + file.getFullPathName().toStdString());
                    return file;
                }
            }
        }
        
        // Third pass: case-insensitive search
        for (const auto& file : allFiles) {
            std::string fileName = file.getFileName().toStdString();
            std::string targetName = sampleName;
            std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
            std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
            if (fileName == targetName) {
                DEBUG_PRINT("Found case-insensitive match in test_samples: " + file.getFullPathName().toStdString());
                return file;
            }
            
            // Also check with extensions in case-insensitive way
            for (const auto& ext : extensions) {
                std::string targetWithExt = targetName + ext;
                if (fileName == targetWithExt) {
                    DEBUG_PRINT("Found case-insensitive match with extension in test_samples: " + file.getFullPathName().toStdString());
                    return file;
                }
            }
        }
        DEBUG_PRINT("No match found in test_samples");
    } else {
        DEBUG_PRINT("Test samples directory does not exist or is not a directory");
    }

    DEBUG_PRINT("File not found anywhere: '" + sampleName + "'");
    return juce::File();
}

juce::File Engine::findVSTFile(const std::string& vstName) const {
    DEBUG_PRINT("findVSTFile called with: '" + vstName + "'");
    DEBUG_PRINT("VST directory: '" + vstDirectory + "'");
    
    if (vstDirectory.empty() || vstName.empty()) {
        DEBUG_PRINT("Empty VST directory or name, returning empty file");
        return juce::File();
    }
    
    juce::File directory(vstDirectory);
    DEBUG_PRINT("Checking VST directory: " + directory.getFullPathName().toStdString());
    if (!directory.exists() || !directory.isDirectory()) {
        DEBUG_PRINT("VST directory does not exist or is not a directory");
        return juce::File();
    }
    
    DEBUG_PRINT("VST directory exists and is valid");
    
    // First try exact filename match (with extension) as a directory
    juce::File exactFile = directory.getChildFile(vstName);
    if (exactFile.exists() && exactFile.isDirectory()) {
        return exactFile;
    }
    
    // If no exact match, try adding .vst3 extension and check if it's a directory
    juce::File file = directory.getChildFile(vstName + ".vst3");
    if (file.exists() && file.isDirectory()) {
        DEBUG_PRINT("Found VST3 bundle: " + file.getFullPathName().toStdString());
        return file;
    }
    
    // If vstName already has an extension, try replacing it with .vst3
    std::string nameWithoutExt = vstName;
    size_t lastDot = nameWithoutExt.find_last_of(".");
    if (lastDot != std::string::npos) {
        nameWithoutExt = nameWithoutExt.substr(0, lastDot);
        DEBUG_PRINT("Trying base name without extension: " + nameWithoutExt);
        
        juce::File file = directory.getChildFile(nameWithoutExt + ".vst3");
        if (file.exists() && file.isDirectory()) {
            DEBUG_PRINT("Found VST3 bundle: " + file.getFullPathName().toStdString());
            return file;
        }
    }
    
    // Comprehensive recursive search for VST3 bundles
    juce::Array<juce::File> allFiles;
    directory.findChildFiles(allFiles, juce::File::findDirectories, true); // true = recursive search, findDirectories = directories only
    DEBUG_PRINT("Found " + std::to_string(allFiles.size()) + " directories in VST directory recursively");
    
    // Debug: print all found directories
    for (const auto& file : allFiles) {
        DEBUG_PRINT("  Found directory: " + file.getFileName().toStdString());
    }
    
    // First pass: exact filename match for directories
    for (const auto& file : allFiles) {
        if (file.getFileName().toStdString() == vstName && file.getFileName().endsWithIgnoreCase(".vst3")) {
            DEBUG_PRINT("Found exact VST3 bundle match: " + file.getFullPathName().toStdString());
            return file;
        }
    }
    
    // Second pass: filename with .vst3 extension  
    std::string targetWithExt = vstName + ".vst3";
    for (const auto& file : allFiles) {
        if (file.getFileName().toStdString() == targetWithExt) {
            DEBUG_PRINT("Found VST3 bundle: " + file.getFullPathName().toStdString());
            return file;
        }
    }
    
    // Second-and-a-half pass: try replacing extensions with .vst3
    if (lastDot != std::string::npos) {
        nameWithoutExt = nameWithoutExt.substr(0, lastDot);
        std::string targetWithNewExt = nameWithoutExt + ".vst3";
        for (const auto& file : allFiles) {
            if (file.getFileName().toStdString() == targetWithNewExt) {
                DEBUG_PRINT("Found VST3 bundle: " + file.getFullPathName().toStdString());
                return file;
            }
        }
    }
    
    // Third pass: case-insensitive search for directories
    for (const auto& file : allFiles) {
        std::string fileName = file.getFileName().toStdString();
        std::string targetName = vstName;
        std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
        std::transform(targetName.begin(), targetName.end(), targetName.begin(), ::tolower);
        if (fileName == targetName && file.getFileName().endsWithIgnoreCase(".vst3")) {
            DEBUG_PRINT("Found case-insensitive VST3 bundle match: " + file.getFullPathName().toStdString());
            return file;
        }
        
        // Also check with .vst3 extension in case-insensitive way
        std::string targetWithExt = targetName + ".vst3";
        if (fileName == targetWithExt) {
            DEBUG_PRINT("Found case-insensitive VST3 bundle: " + file.getFullPathName().toStdString());
            return file;
        }
    }
    
    DEBUG_PRINT("VST file not found anywhere: '" + vstName + "'");
    return juce::File();
}

void Engine::play() {
    if (hasSaved) {
        positionSeconds = savedPosition;
        hasSaved = false;
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
    
    // Pre-generate metronome track (will be muted if disabled)
    generateMetronomeTrack();
    
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
    load(content);
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
        // Always regenerate metronome track when BPM changes
        generateMetronomeTrack();
        
        sendBpmToSynthesizers();
        markStateChanged();
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
    juce::ScopedLock lock(engineStateLock);
    
    if (!currentComposition) {
        currentComposition = std::make_unique<Composition>();
        currentComposition->name = "untitled";
        generateMetronomeTrack();
    }
    
    markStateChanged();
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
        generateMetronomeTrack();
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
    juce::ScopedLock lock(engineStateLock);
    
    if (currentComposition && idx >= 0 && idx < currentComposition->tracks.size()) {
        currentComposition->tracks.erase(currentComposition->tracks.begin() + idx);
        markStateChanged();
    }
}

void Engine::removeTrackByName(const std::string& name) {
    juce::ScopedLock lock(engineStateLock);
    
    if (currentComposition) {
        DEBUG_PRINT("Removing track: " << name);
        for (int i = 0; i < currentComposition->tracks.size(); i++) {
            if (currentComposition->tracks[i]->getName() == name) {
                auto& track = currentComposition->tracks[i];
                currentComposition->tracks.erase(currentComposition->tracks.begin() + i);
                markStateChanged();
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
        markStateChanged();
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
    // Try to acquire lock without blocking audio thread
    if (!engineStateLock.tryEnter()) {
        // If we can't get the lock, output silence to avoid audio glitches
        for (int channel = 0; channel < numOutputChannels; ++channel) {
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);
        }
        return;
    }
    
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
                    // Create a fresh buffer for each track to avoid cross-contamination
                    juce::AudioBuffer<float> isolatedTrackBuffer(2, numSamples);
                    isolatedTrackBuffer.clear();
                    
                    track->applyAutomation(positionSeconds);
                    track->process(positionSeconds, isolatedTrackBuffer, numSamples, sampleRate);
                                 
                    // Check for audio output from this track
                    float trackPeak = 0.0f;
                    for (int ch = 0; ch < 2; ++ch) {
                        for (int i = 0; i < numSamples; ++i) {
                            trackPeak = std::max(trackPeak, std::abs(isolatedTrackBuffer.getSample(ch, i)));
                        }
                    }
                    if (trackPeak > 0.001f) { // Only log if there's significant audio
                        DEBUG_PRINT("Track '" << track->getName() << "' peak: " << trackPeak);
                    }
                    
                    // Mix the stereo track buffer into the output buffer
                    for (int ch = 0; ch < numOutputChannels; ++ch) {
                        int sourceChannel = ch % 2; // Map multi-channel outputs to stereo sources
                        tempMixBuffer.addFrom(ch, 0, isolatedTrackBuffer, sourceChannel, 0, numSamples);
                    }
                }
            }
        }
        if (metronomeTrack) {
            juce::AudioBuffer<float> metronomeBuffer(numOutputChannels, numSamples);
            metronomeBuffer.clear();
            metronomeTrack->process(positionSeconds, metronomeBuffer, numSamples, sampleRate);
            
            // Only mix into output if metronome is enabled
            if (metronomeEnabled) {
                for (int ch = 0; ch < numOutputChannels; ++ch) {
                    tempMixBuffer.addFrom(ch, 0, metronomeBuffer, ch, 0, numSamples);
                }
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
    
    // Release the engine state lock
    engineStateLock.exit();
}

std::string Engine::getStateString() const {
    if (!currentComposition) {
        return "{}";
    }
    
    json engineState;
    engineState["engineState"]["version"] = "1.0";
    engineState["engineState"]["timestamp"] = lastStateChangeTimestamp.count();
    
    // Note: Playback state is intentionally excluded from collaboration
    // Each user should have independent playback position, selected track, etc.
    
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
        
        // Master track automation data - skip default points (single point at -1.0 seconds)
        auto& masterAutomationJson = masterTrackJson["automation"];
        const auto& masterAutomationData = masterTrack->getAutomationData();
        for (const auto& [effectName, parameterMap] : masterAutomationData) {
            auto& effectAutomation = masterAutomationJson[effectName];
            for (const auto& [parameterName, points] : parameterMap) {
                // Skip automation if it only has one point at -1.0 seconds (default state)
                if (points.size() == 1 && points[0].time <= -0.9) {
                    continue; // Skip this automation parameter
                }
                
                auto& pointsArray = effectAutomation[parameterName];
                for (const auto& point : points) {
                    // Skip any points at negative time (default points)
                    if (point.time < 0.0) {
                        continue;
                    }
                    
                    json pointJson;
                    pointJson["time"] = point.time;
                    pointJson["value"] = point.value;
                    pointJson["curve"] = point.curve;
                    pointsArray.push_back(pointJson);
                }
                
                // If we ended up with no valid points, remove this parameter
                if (pointsArray.empty()) {
                    effectAutomation.erase(parameterName);
                }
            }
            
            // If we ended up with no parameters for this effect, remove it
            if (effectAutomation.empty()) {
                masterAutomationJson.erase(effectName);
            }
        }
        
        // Master automated parameters list (for order preservation) - only include params with actual automation
        auto& masterAutomatedParamsJson = masterTrackJson["automatedParameters"];
        const auto& masterAutomatedParams = masterTrack->getAutomatedParameters();
        for (const auto& [effectName, parameterName] : masterAutomatedParams) {
            // Check if this parameter actually has valid automation points (not just default)
            const auto& masterAutomationData = masterTrack->getAutomationData();
            auto effectIt = masterAutomationData.find(effectName);
            if (effectIt != masterAutomationData.end()) {
                auto paramIt = effectIt->second.find(parameterName);
                if (paramIt != effectIt->second.end()) {
                    const auto& points = paramIt->second;
                    // Only include if it has more than one point OR the single point is not at -1.0 seconds
                    if (points.size() > 1 || (points.size() == 1 && points[0].time >= 0.0)) {
                        json paramPair;
                        paramPair["effectName"] = effectName;
                        paramPair["parameterName"] = parameterName;
                        masterAutomatedParamsJson.push_back(paramPair);
                    }
                }
            }
        }
    } else {
        masterTrackJson["name"] = "Master";
        masterTrackJson["volume"] = 0.0;
        masterTrackJson["pan"] = 0.0;
        masterTrackJson["muted"] = false;
        masterTrackJson["soloed"] = false;
        masterTrackJson["effects"] = json::array();
        masterTrackJson["automation"] = json::object();
        masterTrackJson["automatedParameters"] = json::array();
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
        
        // Track type identification
        trackJson["type"] = (track->getType() == Track::TrackType::Audio) ? "audio" : "midi";
        
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
        
        // Track clips - handle both audio and MIDI
        trackJson["clips"] = json::array(); // Initialize as empty array
        auto& clipsJson = trackJson["clips"];
        DEBUG_PRINT("Serializing clips for track: " + track->getName());
        
        if (track->getType() == Track::TrackType::Audio) {
            // Audio clips
            const auto& clips = track->getClips();
            DEBUG_PRINT("  Audio track has " + std::to_string(clips.size()) + " clips");
            for (const auto& clip : clips) {
                json clipJson;
                clipJson["file"] = clip.sourceFile.getFileName().toStdString();
                clipJson["startTime"] = clip.startTime;
                clipJson["offset"] = clip.offset;
                clipJson["duration"] = clip.duration;
                clipJson["volume"] = clip.volume;
                clipsJson.push_back(clipJson);
                DEBUG_PRINT("    Serialized audio clip: " + clip.sourceFile.getFileName().toStdString() + 
                           " at " + std::to_string(clip.startTime));
            }
        } else if (track->getType() == Track::TrackType::MIDI) {
            // MIDI clips
            auto* midiTrack = dynamic_cast<MIDITrack*>(track.get());
            if (midiTrack) {
                const auto& midiClips = midiTrack->getMIDIClips();
                DEBUG_PRINT("  MIDI track has " + std::to_string(midiClips.size()) + " clips");
                for (const auto& clip : midiClips) {
                    json clipJson;
                    if (!clip.sourceFile.getFileName().isEmpty()) {
                        clipJson["file"] = clip.sourceFile.getFileName().toStdString();
                    } else {
                        clipJson["file"] = "";
                    }
                    clipJson["startTime"] = clip.startTime;
                    clipJson["offset"] = clip.offset;
                    clipJson["duration"] = clip.duration;
                    clipJson["velocity"] = clip.velocity;
                    clipJson["channel"] = clip.channel;
                    clipJson["transpose"] = clip.transpose;
                    
                    DEBUG_PRINT("    Serialized MIDI clip at " + std::to_string(clip.startTime) + 
                               " with " + std::to_string(clip.midiData.getNumEvents()) + " MIDI events");
                    
                    // Serialize MIDI data if present
                    if (!clip.midiData.isEmpty()) {
                        auto& midiDataJson = clipJson["midiData"];
                        juce::MidiBuffer::Iterator iterator(clip.midiData);
                        juce::MidiMessage message;
                        int samplePosition;
                        
                        while (iterator.getNextEvent(message, samplePosition)) {
                            json eventJson;
                            eventJson["samplePosition"] = samplePosition;
                            eventJson["rawData"] = std::vector<uint8_t>(message.getRawData(), 
                                                                       message.getRawData() + message.getRawDataSize());
                            midiDataJson.push_back(eventJson);
                        }
                    } else {
                        clipJson["midiData"] = json::array();
                    }
                    
                    clipsJson.push_back(clipJson);
                }
            }
        }
        
        // Track effects and synthesizers
        auto& trackEffects = trackJson["effects"];
        auto& trackSynthesizers = trackJson["synthesizers"];
        const auto& trackEffectsList = const_cast<Track*>(track.get())->getEffects();
        DEBUG_PRINT("Track '" + track->getName() + "' has " + std::to_string(trackEffectsList.size()) + " effects");
        for (const auto& effect : trackEffectsList) {
            if (effect) {
                DEBUG_PRINT("  Effect: " + effect->getName() + ", VST: " + effect->getVSTPath());
                json effectJson;
                effectJson["name"] = effect->getName();
                // Extract filename with extension from the VST path
                juce::File vstFile(effect->getVSTPath());
                effectJson["vstName"] = vstFile.getFileName().toStdString();
                effectJson["vstPath"] = effect->getVSTPath();
                effectJson["enabled"] = effect->enabled();
                effectJson["index"] = effect->getIndex();
                effectJson["isSynthesizer"] = effect->isSynthesizer();
                
                // Effect parameters
                auto& parameters = effectJson["parameters"];
                int numParams = effect->getNumParameters();
                for (int p = 0; p < numParams; ++p) {
                    json paramJson;
                    paramJson["index"] = p;
                    paramJson["value"] = effect->getParameter(p);
                    parameters.push_back(paramJson);
                }
                
                // Add to appropriate array based on type
                if (effect->isSynthesizer()) {
                    trackSynthesizers.push_back(effectJson);
                } else {
                    trackEffects.push_back(effectJson);
                }
            }
        }
        
        // Track automation data - skip default points (single point at -1.0 seconds)
        auto& automationJson = trackJson["automation"];
        const auto& automationData = track->getAutomationData();
        for (const auto& [effectName, parameterMap] : automationData) {
            auto& effectAutomation = automationJson[effectName];
            for (const auto& [parameterName, points] : parameterMap) {
                // Skip automation if it only has one point at -1.0 seconds (default state)
                if (points.size() == 1 && points[0].time <= -0.9) {
                    continue; // Skip this automation parameter
                }
                
                auto& pointsArray = effectAutomation[parameterName];
                for (const auto& point : points) {
                    // Skip any points at negative time (default points)
                    if (point.time < 0.0) {
                        continue;
                    }
                    
                    json pointJson;
                    pointJson["time"] = point.time;
                    pointJson["value"] = point.value;
                    pointJson["curve"] = point.curve;
                    pointsArray.push_back(pointJson);
                }
                
                // If we ended up with no valid points, remove this parameter
                if (pointsArray.empty()) {
                    effectAutomation.erase(parameterName);
                }
            }
            
            // If we ended up with no parameters for this effect, remove it
            if (effectAutomation.empty()) {
                automationJson.erase(effectName);
            }
        }
        
        // Automated parameters list (for order preservation) - only include params with actual automation
        auto& automatedParamsJson = trackJson["automatedParameters"];
        const auto& automatedParams = track->getAutomatedParameters();
        for (const auto& [effectName, parameterName] : automatedParams) {
            // Check if this parameter actually has valid automation points (not just default)
            const auto& automationData = track->getAutomationData();
            auto effectIt = automationData.find(effectName);
            if (effectIt != automationData.end()) {
                auto paramIt = effectIt->second.find(parameterName);
                if (paramIt != effectIt->second.end()) {
                    const auto& points = paramIt->second;
                    // Only include if it has more than one point OR the single point is not at -1.0 seconds
                    if (points.size() > 1 || (points.size() == 1 && points[0].time >= 0.0)) {
                        json paramPair;
                        paramPair["effectName"] = effectName;
                        paramPair["parameterName"] = parameterName;
                        automatedParamsJson.push_back(paramPair);
                    }
                }
            }
        }
        
        tracks.push_back(trackJson);
    }
    
    // Debug: Count total clips across all tracks
    int totalClips = 0;
    for (const auto& track : currentComposition->tracks) {
        if (track->getType() == Track::TrackType::Audio) {
            totalClips += track->getClips().size();
        } else if (track->getType() == Track::TrackType::MIDI) {
            auto* midiTrack = dynamic_cast<MIDITrack*>(track.get());
            if (midiTrack) {
                totalClips += midiTrack->getMIDIClips().size();
            }
        }
    }
    DEBUG_PRINT("SERIALIZATION: Total clips being serialized: " + std::to_string(totalClips));

    return engineState.dump(2); // Pretty print with 2-space indentation
}

void Engine::save(const std::string& path) const {
    DEBUG_PRINT("Engine::save called with path: " << path);
    
    std::string stateString = getStateString();
    
    std::ofstream out(path);
    if (!out.is_open()) {
        DEBUG_PRINT("Failed to open file for writing: " << path);
        return;
    }

    out << stateString;
    out.close();

    DEBUG_PRINT("Engine state written to file: " << path);
}

void Engine::load(const std::string& stateData) {
    juce::ScopedLock lock(engineStateLock);
    
    if (stateData.empty()) {
        DEBUG_PRINT("Engine::loadState called with empty state string");
        return;
    }
    
    DEBUG_PRINT("Engine::loadState called with state size: " + std::to_string(stateData.size()));
    
    // Mark that state is changing
    markStateChanged();
    
    try {
        json parsedState = json::parse(stateData);
        
        if (!parsedState.contains("engineState")) {
            DEBUG_PRINT("ERROR: No engineState section found in JSON");
            return;
        }
        
        const auto& engineState = parsedState["engineState"];
        
        // Skip playback state - keep local playback state independent
        // This allows each user to have their own playback position, selected track, etc.
        
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
                // Regenerate metronome track when BPM is loaded
                generateMetronomeTrack();
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
                            DEBUG_PRINT("Loading master track VST: '" + vstName + "'");
                            
                            // Find VST file by name
                            juce::File vstFile = findVSTFile(vstName);
                            if (vstFile.exists()) {
                                DEBUG_PRINT("Successfully found master track VST file: " + vstFile.getFullPathName().toStdString());
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
                
                // Store master track automation data
                if (masterTrackData.contains("automation")) {
                    const auto& automationJson = masterTrackData["automation"];
                    for (const auto& [effectName, parameterMap] : automationJson.items()) {
                        for (const auto& [parameterName, pointsArray] : parameterMap.items()) {
                            if (pointsArray.is_array()) {
                                // Collect all points for this parameter first
                                std::vector<Track::AutomationPoint> points;
                                for (const auto& pointJson : pointsArray) {
                                    if (pointJson.contains("time") && pointJson.contains("value")) {
                                        Track::AutomationPoint point;
                                        point.time = pointJson["time"].get<double>();
                                        point.value = pointJson["value"].get<float>();
                                        point.curve = pointJson.value("curve", 0.0f);
                                        points.push_back(point);
                                    }
                                }
                                
                                if (!points.empty() && 
                                    (points.size() > 1 || (points.size() == 1 && points[0].time >= 0.0))) {
                                    PendingAutomation pendingAuto;
                                    pendingAuto.trackName = "Master";
                                    pendingAuto.effectName = effectName;
                                    pendingAuto.parameterName = parameterName;
                                    pendingAuto.points = points;
                                    pendingAutomation.push_back(pendingAuto);
                                    DEBUG_PRINT("Stored pending automation for Master track, effect '" + effectName + 
                                               "', parameter '" + parameterName + "' with " + std::to_string(points.size()) + " points");
                                }
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
                    std::string trackType = trackData.value("type", "audio"); // Default to audio for backward compatibility
                    DEBUG_PRINT("Loading track: '" + trackName + "' of type: '" + trackType + "'");
                    
                    std::unique_ptr<Track> track;
                    if (trackType == "midi") {
                        track = std::make_unique<MIDITrack>();
                    } else {
                        track = std::make_unique<AudioTrack>(formatManager);
                    }
                    
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
                                
                                // Only audio tracks can have reference clips
                                if (trackType == "audio") {
                                    auto* audioTrack = dynamic_cast<AudioTrack*>(track.get());
                                    if (audioTrack) {
                                        audioTrack->setReferenceClip(refClip);
                                    }
                                }
                            } else {
                                DEBUG_PRINT("Reference clip file not found: " + fileName);
                            }
                        }
                    }
                    
                    // Load clips - handle both audio and MIDI
                    if (trackData.contains("clips") && trackData["clips"].is_array()) {
                        DEBUG_PRINT("Loading " + std::to_string(trackData["clips"].size()) + " clips for track: " + trackName);
                        for (const auto& clipData : trackData["clips"]) {
                            if (trackType == "audio") {
                                // Load audio clip
                        if (clipData.contains("file")) {
                            std::string fileName = clipData["file"].get<std::string>();
                            DEBUG_PRINT("Loading audio clip file: '" + fileName + "'");
                            juce::File file = findSampleFile(fileName);
                            
                            if (file.existsAsFile()) {
                                DEBUG_PRINT("Successfully found audio clip file: " + file.getFullPathName().toStdString());
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
                                        DEBUG_PRINT("Successfully added audio clip to track: " + trackName);
                                    } else {
                                        DEBUG_PRINT("Audio clip file not found: " + fileName);
                                    }
                                }
                            } else if (trackType == "midi") {
                                // Load MIDI clip
                                auto* midiTrack = dynamic_cast<MIDITrack*>(track.get());
                                if (midiTrack) {
                                    MIDIClip midiClip;
                                    DEBUG_PRINT("Loading MIDI clip for track: " + trackName);
                                    
                                    if (clipData.contains("file") && !clipData["file"].get<std::string>().empty()) {
                                        std::string fileName = clipData["file"].get<std::string>();
                                        juce::File file = findSampleFile(fileName);
                                        if (file.existsAsFile()) {
                                            midiClip.sourceFile = file;
                                            midiClip.loadFromFile(file);
                                        }
                                    }
                                    
                                    if (clipData.contains("startTime")) {
                                        midiClip.startTime = clipData["startTime"].get<double>();
                                    }
                                    if (clipData.contains("offset")) {
                                        midiClip.offset = clipData["offset"].get<double>();
                                    }
                                    if (clipData.contains("duration")) {
                                        midiClip.duration = clipData["duration"].get<double>();
                                    }
                                    if (clipData.contains("velocity")) {
                                        midiClip.velocity = clipData["velocity"].get<float>();
                                    }
                                    if (clipData.contains("channel")) {
                                        midiClip.channel = clipData["channel"].get<int>();
                                    }
                                    if (clipData.contains("transpose")) {
                                        midiClip.transpose = clipData["transpose"].get<int>();
                                    }
                                    
                                    // Load MIDI data
                                    if (clipData.contains("midiData") && clipData["midiData"].is_array()) {
                                        midiClip.midiData.clear();
                                        DEBUG_PRINT("Loading " + std::to_string(clipData["midiData"].size()) + " MIDI events");
                                        for (const auto& eventData : clipData["midiData"]) {
                                            if (eventData.contains("samplePosition") && eventData.contains("rawData")) {
                                                int samplePosition = eventData["samplePosition"].get<int>();
                                                std::vector<uint8_t> rawData = eventData["rawData"].get<std::vector<uint8_t>>();
                                                
                                                if (!rawData.empty()) {
                                                    juce::MidiMessage message(rawData.data(), static_cast<int>(rawData.size()));
                                                    midiClip.midiData.addEvent(message, samplePosition);
                                                }
                                            }
                                        }
                                    }
                                    
                                    midiTrack->addMIDIClip(midiClip);
                                    DEBUG_PRINT("Successfully added MIDI clip to track: " + trackName);
                                }
                            }
                        }
                    }
                    
                    // Load track effects
                    if (trackData.contains("effects") && trackData["effects"].is_array()) {
                        for (const auto& effectData : trackData["effects"]) {
                            if (effectData.contains("vstName") || effectData.contains("vstPath")) {
                                std::string vstName = effectData.value("vstName", "");
                                std::string vstPath = effectData.value("vstPath", "");
                                DEBUG_PRINT("Loading track effect: '" + vstName + "' for track: " + trackName);
                                
                                // Find VST file
                                juce::File vstFile;
                                if (!vstPath.empty() && juce::File(vstPath).existsAsFile()) {
                                    vstFile = juce::File(vstPath);
                                } else if (!vstName.empty()) {
                                    vstFile = findVSTFile(vstName);
                                }
                                
                                if (vstFile.exists()) {
                                    DEBUG_PRINT("Successfully found track effect file: " + vstFile.getFullPathName().toStdString());
                                    // Store effect for deferred loading
                                    PendingEffect pendingEffect;
                                    pendingEffect.trackName = trackName;
                                    pendingEffect.vstPath = vstFile.getFullPathName().toStdString();
                                    pendingEffect.enabled = effectData.value("enabled", true);
                                    pendingEffect.index = effectData.value("index", 0);
                                    
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
                                } else {
                                    DEBUG_PRINT("Effect file not found: " + vstName + " / " + vstPath);
                                }
                            }
                        }
                    }
                    
                    // Check if track has synthesizers to load
                    if (trackData.contains("synthesizers") && trackData["synthesizers"].is_array()) {
                        auto& synthArray = trackData["synthesizers"];
                        DEBUG_PRINT("Found synthesizers array for track: '" + trackName + "' with " + std::to_string(synthArray.size()) + " synthesizers");
                        
                        // Load synthesizers onto this track (not create new tracks)
                        for (const auto& synthData : synthArray) {
                            if (synthData.contains("vstName") || synthData.contains("vstPath")) {
                                std::string vstName = synthData.value("vstName", "");
                                std::string vstPath = synthData.value("vstPath", "");
                                DEBUG_PRINT("Loading synthesizer '" + vstName + "' onto track: '" + trackName + "'");
                                
                                // Find VST file
                                juce::File vstFile;
                                if (!vstPath.empty() && juce::File(vstPath).existsAsFile()) {
                                    vstFile = juce::File(vstPath);
                                } else if (!vstName.empty()) {
                                    vstFile = findVSTFile(vstName);
                                }
                                
                                if (vstFile.exists()) {
                                    DEBUG_PRINT("Successfully found synthesizer file: " + vstFile.getFullPathName().toStdString());
                                    
                                    // Load the synthesizer onto this track
                                    if (track && track.get()) {
                                        if (Effect* synthEffect = track->addEffect(vstFile.getFullPathName().toStdString())) {
                                            DEBUG_PRINT("Successfully loaded synthesizer '" + vstName + "' onto track '" + trackName + "'");
                                            
                                            // Set enabled state
                                            if (synthData.value("enabled", true)) {
                                                synthEffect->enable();
                                            } else {
                                                synthEffect->disable();
                                            }
                                            
                                            // Apply saved parameters
                                            if (synthData.contains("parameters") && synthData["parameters"].is_array()) {
                                                for (const auto& paramData : synthData["parameters"]) {
                                                    if (paramData.contains("index") && paramData.contains("value")) {
                                                        int paramIndex = paramData["index"].get<int>();
                                                        float paramValue = paramData["value"].get<float>();
                                                        synthEffect->setParameter(paramIndex, paramValue);
                                                    }
                                                }
                                            }
                                        } else {
                                            DEBUG_PRINT("Failed to load synthesizer '" + vstName + "' onto track '" + trackName + "'");
                                        }
                                    } else {
                                        DEBUG_PRINT("Track pointer is invalid for synthesizer loading");
                                    }
                                } else {
                                    DEBUG_PRINT("Synthesizer file not found: " + vstName + " / " + vstPath);
                                }
                            }
                        }
                    } else {
                        DEBUG_PRINT("No synthesizers array found for track: '" + trackName + "'");
                    }
                    
                    // Store automation data for deferred loading after effects are processed
                    if (trackData.contains("automation")) {
                        const auto& automationJson = trackData["automation"];
                        for (const auto& [effectName, parameterMap] : automationJson.items()) {
                            for (const auto& [parameterName, pointsArray] : parameterMap.items()) {
                                if (pointsArray.is_array()) {
                                    // Collect all points for this parameter first
                                    std::vector<Track::AutomationPoint> points;
                                    for (const auto& pointJson : pointsArray) {
                                        if (pointJson.contains("time") && pointJson.contains("value")) {
                                            Track::AutomationPoint point;
                                            point.time = pointJson["time"].get<double>();
                                            point.value = pointJson["value"].get<float>();
                                            point.curve = pointJson.value("curve", 0.0f);
                                            points.push_back(point);
                                        }
                                    }
                                    
                                    if (!points.empty() && 
                                        (points.size() > 1 || (points.size() == 1 && points[0].time >= 0.0))) {
                                        PendingAutomation pendingAuto;
                                        pendingAuto.trackName = trackName;
                                        pendingAuto.effectName = effectName;
                                        pendingAuto.parameterName = parameterName;
                                        pendingAuto.points = points;
                                        pendingAutomation.push_back(pendingAuto);
                                        DEBUG_PRINT("Stored pending automation for track '" + trackName + 
                                                   "', effect '" + effectName + "', parameter '" + parameterName + 
                                                   "' with " + std::to_string(points.size()) + " points");
                                    }
                                }
                            }
                        }
                    }
                    
                    // Load automated parameters order (for UI consistency)
                    if (trackData.contains("automatedParameters") && trackData["automatedParameters"].is_array()) {
                        // The automation points are already loaded above, this would be for order preservation
                        // Current Track API doesn't support reordering, but points are loaded correctly
                    }
                    
                    // Prepare track for playback and add to composition
                    track->prepareToPlay(sampleRate, currentBufferSize);
                    
                    // Debug: Count clips on this track
                    int clipCount = 0;
                    if (track->getType() == Track::TrackType::Audio) {
                        clipCount = track->getClips().size();
                    } else if (track->getType() == Track::TrackType::MIDI) {
                        auto* midiTrack = dynamic_cast<MIDITrack*>(track.get());
                        if (midiTrack) {
                            clipCount = midiTrack->getMIDIClips().size();
                        }
                    }
                    DEBUG_PRINT("LOADING: Track '" + trackName + "' loaded with " + std::to_string(clipCount) + " clips");
                    
                    currentComposition->tracks.push_back(std::move(track));
                    DEBUG_PRINT("Track loaded and finalized: " + trackName);
                }
            }
        }
        
        // Prepare master track for playback
        if (masterTrack) {
            masterTrack->prepareToPlay(sampleRate, currentBufferSize);
        }
        
        DEBUG_PRINT("Engine state loaded successfully using nlohmann::json");
        DEBUG_PRINT("Composition: " + (currentComposition ? currentComposition->name : "null"));
        DEBUG_PRINT("Tracks loaded: " + std::to_string(currentComposition ? currentComposition->tracks.size() : 0));
        DEBUG_PRINT("Selected track: " + selectedTrackName);
        
        // Process pending effects and synthesizers
        DEBUG_PRINT("Processing " + std::to_string(pendingEffects.size()) + " pending effects");
        for (const auto& pendingEffect : pendingEffects) {
            // Check if this VST is a synthesizer by loading it temporarily
            juce::File vstFile(pendingEffect.vstPath);
            auto tempEffect = std::make_unique<Effect>();
            if (tempEffect->loadVST(pendingEffect.vstPath)) {
                bool isSynth = tempEffect->isSynthesizer();
                tempEffect.reset(); // Clean up temporary effect
                
                if (isSynth) {
                    // Create a new track for synthesizer
                    DEBUG_PRINT("Creating new track for synthesizer: " + pendingEffect.vstPath);
                    if (currentComposition) {
                        auto newTrack = std::make_unique<AudioTrack>(formatManager);
                        newTrack->setName(pendingEffect.trackName);
                        newTrack->prepareToPlay(sampleRate, currentBufferSize);
                        
                        // Load the synthesizer as an effect on the new track
                        auto synthEffect = newTrack->addEffect(pendingEffect.vstPath);
                        if (synthEffect) {
                            if (pendingEffect.enabled) {
                                synthEffect->enable();
                            } else {
                                synthEffect->disable();
                            }
                            // Apply parameters
                            for (const auto& param : pendingEffect.parameters) {
                                synthEffect->setParameter(param.first, param.second);
                            }
                        }
                        
                        currentComposition->tracks.push_back(std::move(newTrack));
                    }
                } else {
                    // Regular effect - add to existing track
                    if (pendingEffect.trackName == "Master") {
                        // Load effect on master track
                        if (masterTrack) {
                            DEBUG_PRINT("Loading effect on master track: " + pendingEffect.vstPath);
                            auto effect = masterTrack->addEffect(pendingEffect.vstPath);
                            if (effect) {
                                if (pendingEffect.enabled) {
                                    effect->enable();
                                } else {
                                    effect->disable();
                                }
                                // Apply parameters
                                for (const auto& param : pendingEffect.parameters) {
                                    effect->setParameter(param.first, param.second);
                                }
                            }
                        }
                    } else {
                        // Find the track and load effect
                        if (currentComposition) {
                            for (auto& track : currentComposition->tracks) {
                                if (track && track->getName() == pendingEffect.trackName) {
                                    DEBUG_PRINT("Loading effect on track '" + pendingEffect.trackName + "': " + pendingEffect.vstPath);
                                    auto effect = track->addEffect(pendingEffect.vstPath);
                                    if (effect) {
                                        if (pendingEffect.enabled) {
                                            effect->enable();
                                        } else {
                                            effect->disable();
                                        }
                                        // Apply parameters
                                        for (const auto& param : pendingEffect.parameters) {
                                            effect->setParameter(param.first, param.second);
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            } else {
                DEBUG_PRINT("Failed to load VST for pending effect: " + pendingEffect.vstPath);
            }
        }
        pendingEffects.clear();
        
        // Process pending automation after all effects are loaded
        DEBUG_PRINT("Processing " + std::to_string(pendingAutomation.size()) + " pending automation entries");
        for (const auto& pendingAuto : pendingAutomation) {
            Track* targetTrack = nullptr;
            
            if (pendingAuto.trackName == "Master") {
                targetTrack = masterTrack.get();
            } else {
                // Find the track
                if (currentComposition) {
                    for (auto& track : currentComposition->tracks) {
                        if (track && track->getName() == pendingAuto.trackName) {
                            targetTrack = track.get();
                            break;
                        }
                    }
                }
            }
            
            if (targetTrack) {
                DEBUG_PRINT("Applying automation to track '" + pendingAuto.trackName + 
                           "', effect '" + pendingAuto.effectName + "', parameter '" + pendingAuto.parameterName + 
                           "' with " + std::to_string(pendingAuto.points.size()) + " points");
                
                for (const auto& point : pendingAuto.points) {
                    targetTrack->addAutomationPoint(pendingAuto.effectName, pendingAuto.parameterName, point);
                }
            } else {
                DEBUG_PRINT("WARNING: Could not find track '" + pendingAuto.trackName + "' for automation");
            }
        }
        pendingAutomation.clear();
        
        // Send BPM to all loaded synthesizers after project load
        sendBpmToSynthesizers();
        
        // Debug: Count total clips loaded across all tracks
        int totalClipsLoaded = 0;
        for (const auto& t : currentComposition->tracks) {
            totalClipsLoaded += t->getClips().size();
        }
        DEBUG_PRINT("*** LOAD COMPLETE: Total clips loaded across all tracks: " + std::to_string(totalClipsLoaded));
        
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
