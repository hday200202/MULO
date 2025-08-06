#include "Engine.hpp"
#include "../DebugConfig.hpp"

Engine::Engine() {
    formatManager.registerBasicFormats();
    
    deviceManager.initialise(0, 2, nullptr, false);
    
    // Don't force a specific sample rate here - let the application configure it
    // after loading the config file
    auto currentSetup = deviceManager.getAudioDeviceSetup();
    
    currentSetup.bufferSize = 256;
    // Note: Sample rate will be set by application after config is loaded
    
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

    std::string line;
    auto comp = std::make_unique<Composition>();
    std::unique_ptr<Track> currentTrack = nullptr;
    bool inClips = false;
    AudioClip currentClip;
    bool isMasterTrack = false;

    while (std::getline(file, line)) {
        auto trim = [](std::string s) {
            const char* ws = " \t\n\r";
            size_t start = s.find_first_not_of(ws);
            size_t end = s.find_last_not_of(ws);
            return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
        };

        auto extract = [&](const std::string& l) -> std::string {
            auto i = l.find(':');
            if (i == std::string::npos) return "";
            auto val = trim(l.substr(i + 1));
            if (!val.empty() && val.back() == ',') val.pop_back();
            if (!val.empty() && val.front() == '"') val = val.substr(1, val.size() - 2);
            return val;
        };

        line = trim(line);

        if (line.find("\"name\"") != std::string::npos && !inClips && !currentTrack) {
            comp->name = extract(line);
        } else if (line.find("\"bpm\"") != std::string::npos) {
            comp->bpm = std::stod(extract(line));
        } else if (line.find("\"numerator\"") != std::string::npos) {
            comp->timeSigNumerator = std::stoi(extract(line));
        } else if (line.find("\"denominator\"") != std::string::npos) {
            comp->timeSigDenominator = std::stoi(extract(line));
        } else if (line.find("\"tracks\"") != std::string::npos) {
            continue;
        } else if (line.find('{') != std::string::npos && !inClips) {
            currentTrack = std::make_unique<Track>(formatManager);
            isMasterTrack = false;
        } else if (line.find("\"name\"") != std::string::npos && currentTrack && !inClips) {
            std::string name = extract(line);
            currentTrack->setName(name);
            if (name == "Master") {
                isMasterTrack = true;
            }
        } else if (line.find("\"volume\"") != std::string::npos && currentTrack && !inClips) {
            currentTrack->setVolume(std::stof(extract(line)));
        } else if (line.find("\"pan\"") != std::string::npos && currentTrack && !inClips) {
            currentTrack->setPan(std::stof(extract(line)));
        } else if (line.find("\"clips\"") != std::string::npos) {
            inClips = true;
        } else if (inClips && line.find("\"file\"") != std::string::npos) {
            currentClip.sourceFile = juce::File(extract(line));
        } else if (inClips && line.find("\"start\"") != std::string::npos) {
            currentClip.startTime = std::stod(extract(line));
        } else if (inClips && line.find("\"offset\"") != std::string::npos) {
            currentClip.offset = std::stod(extract(line));
        } else if (inClips && line.find("\"duration\"") != std::string::npos) {
            currentClip.duration = std::stod(extract(line));
        } else if (inClips && line.find("\"volume\"") != std::string::npos) {
            currentClip.volume = std::stof(extract(line));
        } else if (inClips && line.find('}') != std::string::npos) {
            if (!isMasterTrack && currentTrack) {
                currentTrack->addClip(currentClip);
            }
            currentClip = AudioClip(); // reset
        } else if (!inClips && line.find('}') != std::string::npos && currentTrack) {
            if (isMasterTrack) {
                masterTrack->setName(currentTrack->getName());
                masterTrack->setVolume(currentTrack->getVolume());
                masterTrack->setPan(currentTrack->getPan());
            } else {
                if (!currentTrack->getName().empty()) {
                    if (!currentTrack->getClips().empty()) {
                        currentTrack->setReferenceClip(currentTrack->getClips()[0]);
                    }
                    comp->tracks.push_back(std::move(currentTrack));
                }
            }
            currentTrack = nullptr;
            isMasterTrack = false;
        } else if (inClips && line.find(']') != std::string::npos) {
            inClips = false;
        }
    }

    currentComposition = std::move(comp);
    DEBUG_PRINT("Loaded composition: " << currentComposition->name);
}

void Engine::saveComposition(const std::string&) {}

std::pair<int, int> Engine::getTimeSignature() const {
    return {currentComposition->timeSigNumerator, currentComposition->timeSigDenominator};
}

double Engine::getBpm() const { return currentComposition->bpm; }

void Engine::setBpm(double newBpm) { currentComposition->bpm = newBpm; }

void Engine::addTrack(const std::string& name, const std::string& samplePath) {
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

std::string escapeMpfString(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}

bool Engine::saveState(const std::string& path) const {
    if (!currentComposition) return false;
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"composition\": {\n";
    ss << "    \"name\": \"" << escapeMpfString(currentComposition->name) << "\",\n";
    ss << "    \"bpm\": " << currentComposition->bpm << ",\n";
    ss << "    \"timeSignature\": {\n";
    ss << "      \"numerator\": " << currentComposition->timeSigNumerator << ",\n";
    ss << "      \"denominator\": " << currentComposition->timeSigDenominator << "\n";
    ss << "    },\n";
    ss << "    \"tracks\": [\n";
    if (masterTrack) {
        ss << "      {\n";
        ss << "        \"name\": \"" << escapeMpfString(masterTrack->getName()) << "\",\n";
        ss << "        \"volume\": " << masterTrack->getVolume() << ",\n";
        ss << "        \"pan\": " << masterTrack->getPan() << ",\n";
        ss << "        \"clips\": []\n";
        ss << "      }" << (currentComposition->tracks.empty() ? "" : ",") << "\n";
    }
    for (size_t i = 0; i < currentComposition->tracks.size(); ++i) {
        const auto* track = currentComposition->tracks[i].get();
        ss << "      {\n";
        ss << "        \"name\": \"" << escapeMpfString(track->getName()) << "\",\n";
        ss << "        \"volume\": " << track->getVolume() << ",\n";
        ss << "        \"pan\": " << track->getPan() << ",\n";
        ss << "        \"clips\": [\n";
        const auto& clips = track->getClips();
        for (size_t j = 0; j < clips.size(); ++j) {
            const auto& clip = clips[j];
            ss << "          {\n";
            ss << "            \"file\": \"" << escapeMpfString(clip.sourceFile.getFullPathName().toStdString()) << "\",\n";
            ss << "            \"start\": " << clip.startTime << ",\n";
            ss << "            \"offset\": " << clip.offset << ",\n";
            ss << "            \"duration\": " << clip.duration << ",\n";
            ss << "            \"volume\": " << clip.volume << "\n";
            ss << "          }" << (j < clips.size() - 1 ? "," : "") << "\n";
        }
        ss << "        ]\n";
        ss << "      }" << (i < currentComposition->tracks.size() - 1 ? "," : "") << "\n";
    }
    ss << "    ]\n";
    ss << "  }\n";
    ss << "}\n";

    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << ss.str();
    out.close();
    
    return true;
}

std::string Engine::getStateString() const {
    if (!currentComposition) return "";
    std::ostringstream ss;
    return ss.str();
}

void Engine::loadState(const std::string& state) {
    std::istringstream file(state);
    auto comp = std::make_unique<Composition>();
    std::unique_ptr<Track> currentTrack = nullptr;
    currentComposition = std::move(comp);
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