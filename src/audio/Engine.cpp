#include "Engine.hpp"

//==============================================================================
// ENGINE 
//==============================================================================

Engine::Engine() {
    formatManager.registerBasicFormats();
    deviceManager.initialise(0, 2, nullptr, true);
    deviceManager.addAudioCallback(this);

    masterTrack = std::make_unique<Track>(formatManager);
    masterTrack->setName("Master");
}

Engine::~Engine() {
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
    std::string currentKey;
    std::unique_ptr<Composition> comp = std::make_unique<Composition>();
    Track* currentTrack = nullptr;
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
        }
        else if (line.find("\"bpm\"") != std::string::npos) {
            comp->bpm = std::stod(extract(line));
        }
        else if (line.find("\"numerator\"") != std::string::npos) {
            comp->timeSigNumerator = std::stoi(extract(line));
        }
        else if (line.find("\"denominator\"") != std::string::npos) {
            comp->timeSigDenominator = std::stoi(extract(line));
        }
        else if (line.find("\"tracks\"") != std::string::npos) {
            continue; // skip
        }
        else if (line.find("{") != std::string::npos && !inClips) {
            currentTrack = new Track(formatManager);
            isMasterTrack = false;
        }
        else if (line.find("\"name\"") != std::string::npos && currentTrack && !inClips) {
            std::string name = extract(line);
            currentTrack->setName(name);
            if (name == "Master") {
                isMasterTrack = true;
            }
        }
        else if (line.find("\"volume\"") != std::string::npos && currentTrack && !inClips) {
            currentTrack->setVolume(std::stof(extract(line)));
        }
        else if (line.find("\"pan\"") != std::string::npos && currentTrack && !inClips) {
            currentTrack->setPan(std::stof(extract(line)));
        }
        else if (line.find("\"clips\"") != std::string::npos) {
            inClips = true;
        }
        else if (inClips && line.find("\"file\"") != std::string::npos) {
            currentClip.sourceFile = juce::File(extract(line));
        }
        else if (inClips && line.find("\"start\"") != std::string::npos) {
            currentClip.startTime = std::stod(extract(line));
        }
        else if (inClips && line.find("\"offset\"") != std::string::npos) {
            currentClip.offset = std::stod(extract(line));
        }
        else if (inClips && line.find("\"duration\"") != std::string::npos) {
            currentClip.duration = std::stod(extract(line));
        }
        else if (inClips && line.find("\"volume\"") != std::string::npos) {
            currentClip.volume = std::stof(extract(line));
        }
        else if (inClips && line.find("}") != std::string::npos) {
            if (!isMasterTrack) {
                currentTrack->addClip(currentClip);
            }
            currentClip = AudioClip(); // reset
        }
        else if (!inClips && line.find("}") != std::string::npos && currentTrack) {
            if (isMasterTrack) {
                masterTrack->setName(currentTrack->getName());
                masterTrack->setVolume(currentTrack->getVolume());
                masterTrack->setPan(currentTrack->getPan());
                delete currentTrack;
            } 
            else {
                if (!currentTrack->getName().empty())
                    comp->tracks.push_back(currentTrack);
                else
                    delete currentTrack;
            }
            currentTrack = nullptr;
            isMasterTrack = false;
        }
        else if (inClips && line.find("]") != std::string::npos) {
            inClips = false;
        }
    }

    currentComposition = std::move(comp);
    std::cout << "Loaded composition: " << currentComposition->name << "\n";
}

void Engine::saveComposition(const std::string&) {}

void Engine::addTrack(const std::string& name) {
    auto* t = new Track(formatManager);
    t->setName(name);
    currentComposition->tracks.push_back(t);
}

void Engine::removeTrack(int idx) {
    if (!currentComposition)
        return;

    if (idx >= 0 && idx < currentComposition->tracks.size()) {
        delete currentComposition->tracks[idx];
        currentComposition->tracks.erase(currentComposition->tracks.begin() + idx);
    }
}

std::string Engine::getCurrentCompositionName() const {
    return currentComposition->name;
}

Track* Engine::getTrack(int idx) {
    if (!currentComposition)
        return nullptr;

    if (idx >= 0 && idx < currentComposition->tracks.size())
        return currentComposition->tracks[idx];

    return nullptr;
}

std::vector<Track*>& Engine::getAllTracks() {
    return currentComposition->tracks;
}

Track* Engine::getMasterTrack() {
    return masterTrack.get();
}

void Engine::audioDeviceIOCallbackWithContext(
    const float* const*,
    int numInputChannels,
    float* const* outputChannelData,
    int numOutputChannels,
    int numSamples,
    const juce::AudioIODeviceCallbackContext&
) {
    juce::AudioBuffer<float> out(outputChannelData, numOutputChannels, numSamples);
    out.clear();

    if (playing) {
        processBlock(out, numSamples);
        positionSeconds += (double)numSamples / sampleRate;
    }
}

// Helper for escaping strings for .mpf format
std::string escapeMpfString(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\' || c == '"') out += '\\';
        out += c;
    }
    return out;
}

// Save the current engine state to a .mpf project file
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
    // Serialize the master track first
    if (masterTrack) {
        ss << "      {\n";
        ss << "        \"name\": \"" << escapeMpfString(masterTrack->getName()) << "\",\n";
        ss << "        \"volume\": " << masterTrack->getVolume() << ",\n";
        ss << "        \"pan\": " << masterTrack->getPan() << ",\n";
        ss << "        \"clips\": [\n";
        ss << "        ]\n";
        ss << "      }" << (currentComposition->tracks.size() > 0 ? "," : "") << "\n";
    }
    // Serialize all other tracks
    for (size_t i = 0; i < currentComposition->tracks.size(); ++i) {
        const auto* track = currentComposition->tracks[i];
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
            ss << "          }" << (j + 1 < clips.size() ? "," : "") << "\n";
        }
        ss << "        ]\n";
        ss << "      }" << (i + 1 < currentComposition->tracks.size() ? "," : "") << "\n";
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
    ss << "{\n";
    ss << "  \"composition\": {\n";
    ss << "    \"name\": \"" << escapeMpfString(currentComposition->name) << "\",\n";
    ss << "    \"bpm\": " << currentComposition->bpm << ",\n";
    ss << "    \"timeSignature\": {\n";
    ss << "      \"numerator\": " << currentComposition->timeSigNumerator << ",\n";
    ss << "      \"denominator\": " << currentComposition->timeSigDenominator << "\n";
    ss << "    },\n";
    ss << "    \"tracks\": [\n";
    // Serialize the master track first
    if (masterTrack) {
        ss << "      {\n";
        ss << "        \"name\": \"" << escapeMpfString(masterTrack->getName()) << "\",\n";
        ss << "        \"volume\": " << masterTrack->getVolume() << ",\n";
        ss << "        \"pan\": " << masterTrack->getPan() << ",\n";
        ss << "        \"clips\": [\n";
        ss << "        ]\n";
        ss << "      }" << (currentComposition->tracks.size() > 0 ? "," : "") << "\n";
    }
    // Serialize all other tracks
    for (size_t i = 0; i < currentComposition->tracks.size(); ++i) {
        const auto* track = currentComposition->tracks[i];
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
            ss << "          }" << (j + 1 < clips.size() ? "," : "") << "\n";
        }
        ss << "        ]\n";
        ss << "      }" << (i + 1 < currentComposition->tracks.size() ? "," : "") << "\n";
    }
    ss << "    ]\n";
    ss << "  }\n";
    ss << "}\n";
    return ss.str();
}

void Engine::loadState(const std::string& state) {
    std::istringstream file(state);

    std::string line;
    std::string currentKey;
    std::unique_ptr<Composition> comp = std::make_unique<Composition>();
    Track* currentTrack = nullptr;
    bool inClips = false;
    AudioClip currentClip;
    bool isMasterTrack = false;

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

    while (std::getline(file, line)) {
        line = trim(line);

        if (line.find("\"name\"") != std::string::npos && !inClips && !currentTrack) {
            comp->name = extract(line);
        }
        else if (line.find("\"bpm\"") != std::string::npos) {
            comp->bpm = std::stod(extract(line));
        }
        else if (line.find("\"numerator\"") != std::string::npos) {
            comp->timeSigNumerator = std::stoi(extract(line));
        }
        else if (line.find("\"denominator\"") != std::string::npos) {
            comp->timeSigDenominator = std::stoi(extract(line));
        }
        else if (line.find("\"tracks\"") != std::string::npos) {
            continue; // skip
        }
        else if (line.find("{") != std::string::npos && !inClips) {
            currentTrack = new Track(formatManager);
            isMasterTrack = false;
        }
        else if (line.find("\"name\"") != std::string::npos && currentTrack && !inClips) {
            std::string name = extract(line);
            currentTrack->setName(name);
            if (name == "Master") {
                isMasterTrack = true;
            }
        }
        else if (line.find("\"volume\"") != std::string::npos && currentTrack && !inClips) {
            currentTrack->setVolume(std::stof(extract(line)));
        }
        else if (line.find("\"pan\"") != std::string::npos && currentTrack && !inClips) {
            currentTrack->setPan(std::stof(extract(line)));
        }
        else if (line.find("\"clips\"") != std::string::npos) {
            inClips = true;
        }
        else if (inClips && line.find("\"file\"") != std::string::npos) {
            currentClip.sourceFile = juce::File(extract(line));
        }
        else if (inClips && line.find("\"start\"") != std::string::npos) {
            currentClip.startTime = std::stod(extract(line));
        }
        else if (inClips && line.find("\"offset\"") != std::string::npos) {
            currentClip.offset = std::stod(extract(line));
        }
        else if (inClips && line.find("\"duration\"") != std::string::npos) {
            currentClip.duration = std::stod(extract(line));
        }
        else if (inClips && line.find("\"volume\"") != std::string::npos) {
            currentClip.volume = std::stof(extract(line));
        }
        else if (inClips && line.find("}") != std::string::npos) {
            if (!isMasterTrack) {
                currentTrack->addClip(currentClip);
            }
            currentClip = AudioClip(); // reset
        }
        else if (!inClips && line.find("}") != std::string::npos && currentTrack) {
            if (isMasterTrack) {
                masterTrack->setName(currentTrack->getName());
                masterTrack->setVolume(currentTrack->getVolume());
                masterTrack->setPan(currentTrack->getPan());
                delete currentTrack;
            } 
            else {
                if (!currentTrack->getName().empty())
                    comp->tracks.push_back(currentTrack);
                else
                    delete currentTrack;
            }
            currentTrack = nullptr;
            isMasterTrack = false;
        }
        else if (inClips && line.find("]") != std::string::npos) {
            inClips = false;
        }
    }

    currentComposition = std::move(comp);
    std::cout << "Loaded state from string: " << currentComposition->name << "\n";
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
    // Clear output
    outputBuffer.clear();

    // Mix all tracks into tempMixBuffer
    tempMixBuffer.setSize(outputBuffer.getNumChannels(), numSamples, false, false, true);
    tempMixBuffer.clear();

    for (auto* track : currentComposition->tracks) {
        if (track && !track->isMuted())
            track->process(positionSeconds, tempMixBuffer, numSamples, sampleRate);
    }

    // Now process the master track (for effects, volume, etc.)
    if (masterTrack)
        masterTrack->process(0.0, tempMixBuffer, numSamples, sampleRate);

    // Copy to output
    outputBuffer.makeCopyOf(tempMixBuffer);
}


//==============================================================================
// COMPOSITION 
//==============================================================================

Composition::Composition() {}

Composition::~Composition() {
    for (auto* t : tracks)
        delete t;
}

Composition::Composition(const std::string&) {}


//==============================================================================
// TRACK 
//==============================================================================

Track::Track(juce::AudioFormatManager& fm) : formatManager(fm) {}

Track::~Track() {}

void Track::setName(const std::string& n) {
    name = n;
}

std::string Track::getName() const {
    return name;
}

void Track::setVolume(float db) {
    volumeDb = db;
}

float Track::getVolume() const {
    return volumeDb;
}

void Track::setPan(float p) {
    pan = juce::jlimit(-1.f, 1.f, p);
}

float Track::getPan() const {
    return pan;
}

void Track::addClip(const AudioClip& c) {
    clips.push_back(c);
}

void Track::removeClip(int idx) {
    if (idx >= 0 && idx < clips.size())
        clips.erase(clips.begin() + idx);
}

const std::vector<AudioClip>& Track::getClips() const {
    return clips;
}

void Track::process(double playheadSeconds,
                    juce::AudioBuffer<float>& output,
                    int numSamples,
                    double sampleRate) {
    const AudioClip* active = nullptr;

    // Determine the active clip for the current audio block
    for (const auto& c : clips) {
        if (playheadSeconds < c.startTime + c.duration && // Check if clip ends after current playhead
            playheadSeconds + (double)numSamples / sampleRate > c.startTime) // Check if clip starts before current block ends
        {
            active = &c;
            break;
        }
    }

    if (!active)
        return; // No active clip for this block

    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        formatManager.createReaderFor(active->sourceFile));

    if (!reader)
        return;

    // Calculate block times within global timeline
    double blockStartTimeSeconds = playheadSeconds;
    double blockEndTimeSeconds = playheadSeconds + (double)numSamples / sampleRate;

    // Calculate precise start and end times within the clip's own timeline for this block
    double readStartTimeInClip = juce::jmax(0.0, blockStartTimeSeconds - active->startTime);
    double readEndTimeInClip = juce::jmin(active->duration, blockEndTimeSeconds - active->startTime);

    if (readStartTimeInClip >= active->duration || readEndTimeInClip <= 0.0) {
        return; // Clip not relevant to current block, or already ended/not started
    }

    // Convert times to sample positions in the source file
    juce::int64 sourceFileStartSample = static_cast<juce::int64>((active->offset + readStartTimeInClip) * reader->sampleRate);
    juce::int64 sourceFileEndSample = static_cast<juce::int64>((active->offset + readEndTimeInClip) * reader->sampleRate);
    juce::int64 numSamplesToRead = sourceFileEndSample - sourceFileStartSample;

    // Calculate where in the output buffer this data should be written
    juce::int64 outputBufferStartSample = static_cast<juce::int64>(juce::jmax(0.0, (active->startTime - blockStartTimeSeconds) * sampleRate));
    juce::int64 numSamplesToWrite = juce::jmin((juce::int64)numSamples - outputBufferStartSample, numSamplesToRead);
    
    if (numSamplesToRead <= 0 || numSamplesToWrite <= 0) {
        return;
    }
    
    outputBufferStartSample = juce::jmax((juce::int64)0, outputBufferStartSample);

    // Read audio data from the clip's source file
    juce::AudioBuffer<float> tempClipBuf(reader->numChannels, (int)numSamplesToRead);
    tempClipBuf.clear();
    reader->read(&tempClipBuf, 0, (int)numSamplesToRead, sourceFileStartSample, true, true);

    // Apply track volume, clip volume, and pan, then mix into the output buffer
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
