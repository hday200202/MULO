#include "Engine.hpp"

//==============================================================================
// ENGINE 
//==============================================================================

Engine::Engine() {
    formatManager.registerBasicFormats();
    deviceManager.initialise(0, 2, nullptr, false);
    deviceManager.addAudioCallback(this);

    masterTrack = std::make_unique<Track>(formatManager);
    masterTrack->setName("Master");
}

Engine::~Engine() {
    deviceManager.closeAudioDevice();
    deviceManager.removeAudioCallback(this);
}

void Engine::play() {
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
    std::cout << "Loaded composition: " << currentComposition->name << "\n";
}

void Engine::saveComposition(const std::string&) {}

std::pair<int, int> Engine::getTimeSignature() const {
    return {currentComposition->timeSigNumerator, currentComposition->timeSigDenominator};
}

double Engine::getBpm() const { return currentComposition->bpm; }

void Engine::setBpm(double newBpm) { currentComposition->bpm = newBpm; }

void Engine::addTrack(const std::string& name, const std::string& samplePath) {
    // Ensure unique track name
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
    tempMixBuffer.setSize(device->getOutputChannelNames().size(), 512);
    tempMixBuffer.clear();
    positionSeconds = 0.0;
    DBG("Device about to start with SR: " << sampleRate);
}

void Engine::audioDeviceStopped() {
    tempMixBuffer.setSize(0, 0);
}

void Engine::processBlock(juce::AudioBuffer<float>& outputBuffer, int numSamples) {
    outputBuffer.clear();
    tempMixBuffer.setSize(outputBuffer.getNumChannels(), numSamples, false, false, true);
    tempMixBuffer.clear();

    if (!currentComposition) return;

    bool anyTrackSoloed = masterTrack && masterTrack->isSolo();
    if (!anyTrackSoloed) {
        for (const auto& track : currentComposition->tracks) {
            if (track && track->isSolo()) {
                anyTrackSoloed = true;
                break;
            }
        }
    }

    // Process all regular tracks into the temporary mixing buffer
    for (const auto& track : currentComposition->tracks) {
        if (track) {
            bool shouldPlay = !anyTrackSoloed ? !track->isMuted() : track->isSolo();
            if (shouldPlay) {
                track->process(positionSeconds, tempMixBuffer, numSamples, sampleRate);
            }
        }
    }

    if (masterTrack && !masterTrack->isMuted()) {
        float masterGain = juce::Decibels::decibelsToGain(masterTrack->getVolume());
        float masterPan = masterTrack->getPan();

        // Apply a simple linear pan law for stereo output
        float panLeft = std::sqrt(0.5f * (1.0f - masterPan));
        float panRight = std::sqrt(0.5f * (1.0f + masterPan));
        
        if (tempMixBuffer.getNumChannels() >= 2) {
            tempMixBuffer.applyGain(0, 0, numSamples, masterGain * panLeft);
            tempMixBuffer.applyGain(1, 0, numSamples, masterGain * panRight);
        } 
        else if (tempMixBuffer.getNumChannels() == 1) {
            tempMixBuffer.applyGain(0, 0, numSamples, masterGain);
        }

        outputBuffer.makeCopyOf(tempMixBuffer);
    }
}

//==============================================================================
// COMPOSITION 
//==============================================================================

Composition::Composition() {}
Composition::~Composition() {}
Composition::Composition(const std::string&) {}

//==============================================================================
// TRACK 
//==============================================================================

Track::Track(juce::AudioFormatManager& fm) : formatManager(fm) {}
Track::~Track() {} // unique_ptr handles referenceClip deletion

void Track::setName(const std::string& n) { name = n; }
std::string Track::getName() const { return name; }
void Track::setVolume(float db) { volumeDb = db; }
float Track::getVolume() const { return volumeDb; }
void Track::setPan(float p) { pan = juce::jlimit(-1.f, 1.f, p); }
float Track::getPan() const { return pan; }
void Track::addClip(const AudioClip& c) { clips.push_back(c); }
void Track::removeClip(int idx) {
    if (idx >= 0 && idx < clips.size()) {
        clips.erase(clips.begin() + idx);
    }
}
const std::vector<AudioClip>& Track::getClips() const { return clips; }

void Track::setReferenceClip(const AudioClip& clip) {
    referenceClip = std::make_unique<AudioClip>(clip);
}

AudioClip* Track::getReferenceClip() {
    return referenceClip.get();
}

void Track::process(double playheadSeconds,
                    juce::AudioBuffer<float>& output,
                    int numSamples,
                    double sampleRate) {
    const AudioClip* active = nullptr;

    for (const auto& c : clips) {
        if (playheadSeconds < c.startTime + c.duration &&
            playheadSeconds + (double)numSamples / sampleRate > c.startTime)
        {
            active = &c;
            break;
        }
    }

    if (!active)
        return;

    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        formatManager.createReaderFor(active->sourceFile));

    if (!reader) {
        DBG("Failed to create reader for: " << active->sourceFile.getFullPathName());
        return;
    }

    double blockStartTimeSeconds = playheadSeconds;
    double blockEndTimeSeconds = playheadSeconds + (double)numSamples / sampleRate;

    double readStartTimeInClip = juce::jmax(0.0, blockStartTimeSeconds - active->startTime);
    double readEndTimeInClip = juce::jmin(active->duration, blockEndTimeSeconds - active->startTime);

    if (readStartTimeInClip >= active->duration || readEndTimeInClip <= 0.0) {
        return;
    }

    juce::int64 sourceFileStartSample = static_cast<juce::int64>((active->offset + readStartTimeInClip) * reader->sampleRate);
    juce::int64 sourceFileEndSample = static_cast<juce::int64>((active->offset + readEndTimeInClip) * reader->sampleRate);
    juce::int64 numSamplesToRead = sourceFileEndSample - sourceFileStartSample;

    juce::int64 outputBufferStartSample = static_cast<juce::int64>(juce::jmax(0.0, (active->startTime - blockStartTimeSeconds) * sampleRate));
    juce::int64 numSamplesToWrite = juce::jmin((juce::int64)numSamples - outputBufferStartSample, numSamplesToRead);
    
    if (numSamplesToRead <= 0 || numSamplesToWrite <= 0) {
        return;
    }
    
    outputBufferStartSample = juce::jmax((juce::int64)0, outputBufferStartSample);

    juce::AudioBuffer<float> tempClipBuf(reader->numChannels, (int)numSamplesToRead);
    tempClipBuf.clear();
    reader->read(&tempClipBuf, 0, (int)numSamplesToRead, sourceFileStartSample, true, true);

    float gain = juce::Decibels::decibelsToGain(volumeDb) * active->volume;

    for (int ch = 0; ch < output.getNumChannels(); ++ch) {
        float panL = (1.0f - juce::jlimit(-1.f, 1.f, pan)) * 0.5f;
        float panR = (1.0f + juce::jlimit(-1.f, 1.f, pan)) * 0.5f;
        float panGain = (ch == 0) ? panL : panR;
        
        output.addFrom(ch, (int)outputBufferStartSample,
                       tempClipBuf, ch % tempClipBuf.getNumChannels(),
                       0, (int)numSamplesToWrite,
                       gain * panGain);
    }
}

//==============================================================================
// AUDIO CLIP 
//==============================================================================
AudioClip::AudioClip()
: startTime(0.0), offset(0.0), duration(0.0), volume(1.0f) {}

AudioClip::AudioClip(
    const juce::File& sourceFile, 
    double startTime, 
    double offset, 
    double duration, 
    float volume
) : sourceFile(sourceFile), startTime(startTime), offset(offset), duration(duration), volume(volume) {}
