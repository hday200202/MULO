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
    std::ifstream in(path);
    if (!in.is_open()) return;

    // Simple JSON parsing (not robust, but works for our format)
    std::string line;
    std::string compName;
    double bpm = 120;
    int tsNum = 4, tsDen = 4;
    std::vector<Track*> loadedTracks;

    auto trim = [](std::string s) {
        size_t start = s.find_first_not_of(" \t\n\r");
        size_t end = s.find_last_not_of(" \t\n\r");
        return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    };

    Track* currentTrack = nullptr;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.find("\"name\":") != std::string::npos && compName.empty()) {
            // Composition name
            auto pos = line.find(":");
            compName = trim(line.substr(pos + 1));
            // Remove leading/trailing quotes and trailing comma
            if (!compName.empty() && compName.front() == '"') compName = compName.substr(1);
            if (!compName.empty() && (compName.back() == '"' || compName.back() == ',')) compName.pop_back();
            if (!compName.empty() && compName.back() == '"') compName.pop_back(); // In case both
        } else if (line.find("\"bpm\":") != std::string::npos) {
            auto pos = line.find(":");
            bpm = std::stod(trim(line.substr(pos + 1)));
        } else if (line.find("\"numerator\":") != std::string::npos) {
            auto pos = line.find(":");
            tsNum = std::stoi(trim(line.substr(pos + 1)));
        } else if (line.find("\"denominator\":") != std::string::npos) {
            auto pos = line.find(":");
            tsDen = std::stoi(trim(line.substr(pos + 1)));
        } else if (line.find("\"name\":") != std::string::npos && !compName.empty()) {
            // Track name
            auto pos = line.find(":");
            std::string tname = trim(line.substr(pos + 1));
            // Remove leading/trailing quotes and trailing comma
            if (!tname.empty() && tname.front() == '"') tname = tname.substr(1);
            if (!tname.empty() && (tname.back() == '"' || tname.back() == ',')) tname.pop_back();
            if (!tname.empty() && tname.back() == '"') tname.pop_back(); // In case both
            currentTrack = new Track(formatManager);
            currentTrack->setName(tname);
            loadedTracks.push_back(currentTrack);
        } else if (line.find("\"volume\":") != std::string::npos && currentTrack) {
            auto pos = line.find(":");
            currentTrack->setVolume(std::stof(trim(line.substr(pos + 1)).c_str()));
        } else if (line.find("\"pan\":") != std::string::npos && currentTrack) {
            auto pos = line.find(":");
            currentTrack->setPan(std::stof(trim(line.substr(pos + 1)).c_str()));
        } else if (line.find("\"file\":") != std::string::npos && currentTrack) {
            // Start of a clip
            juce::File file;
            double start = 0, offset = 0, duration = 0;
            float vol = 1.0f;
            // File
            auto pos = line.find(":");
            std::string fpath = trim(line.substr(pos + 1));
            if (!fpath.empty() && fpath.front() == '"') fpath = fpath.substr(1, fpath.size() - 2);
            file = juce::File(fpath);
            // Next lines: start, offset, duration, volume
            std::getline(in, line); line = trim(line);
            if (line.find("\"start\":") != std::string::npos) {
                pos = line.find(":");
                start = std::stod(trim(line.substr(pos + 1)));
            }
            std::getline(in, line); line = trim(line);
            if (line.find("\"offset\":") != std::string::npos) {
                pos = line.find(":");
                offset = std::stod(trim(line.substr(pos + 1)));
            }
            std::getline(in, line); line = trim(line);
            if (line.find("\"duration\":") != std::string::npos) {
                pos = line.find(":");
                duration = std::stod(trim(line.substr(pos + 1)));
            }
            std::getline(in, line); line = trim(line);
            if (line.find("\"volume\":") != std::string::npos) {
                pos = line.find(":");
                vol = std::stof(trim(line.substr(pos + 1)));
            }
            currentTrack->addClip(AudioClip(file, start, offset, duration, vol));
        }
    }

    // Clean up old tracks before replacing composition
    if (currentComposition) {
        for (auto* t : currentComposition->tracks) delete t;
        currentComposition->tracks.clear();
    }

    // Replace current composition
    currentComposition = std::make_unique<Composition>();
    currentComposition->name = compName;
    currentComposition->bpm = bpm;
    currentComposition->timeSigNumerator = tsNum;
    currentComposition->timeSigDenominator = tsDen;

    // Only add Master if not present in loaded tracks
    bool hasMaster = false;
    for (auto* t : loadedTracks) {
        if (t && t->getName() == "Master") {
            hasMaster = true;
            break;
        }
    }
    if (!hasMaster) {
        addTrack("Master");
    }
    for (auto& t : loadedTracks) {
        currentComposition->tracks.push_back(t);
    }
    loadedTracks.clear();
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

void Engine::loadCompositionFromState(const std::string& state) {
    std::cout << "Loaded state: " << state << std::endl;
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

    for (const auto& c : clips) {
        if (playheadSeconds >= c.startTime &&
            playheadSeconds < c.startTime + c.duration) {
            active = &c;
            break;
        }
    }

    if (!active)
        return;

    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        formatManager.createReaderFor(active->sourceFile));

    if (!reader)
        return;

    double clipTime = playheadSeconds - active->startTime;
    double filePos = active->offset + clipTime;
    juce::int64 startSample = static_cast<juce::int64>(filePos * reader->sampleRate);

    juce::AudioBuffer<float> clipBuf((int)reader->numChannels, numSamples);
    clipBuf.clear();

    reader->read(&clipBuf, 0, numSamples, startSample, true, true);

    float gain = juce::Decibels::decibelsToGain(volumeDb) * active->volume;

    for (int ch = 0; ch < output.getNumChannels(); ++ch) {
        float panL = (1.0f - juce::jlimit(-1.f, 1.f, pan)) * 0.5f;
        float panR = (1.0f + juce::jlimit(-1.f, 1.f, pan)) * 0.5f;
        float panGain = (ch == 0) ? panL : panR;

        output.addFrom(ch, 0,
                       clipBuf, ch % clipBuf.getNumChannels(),
                       0, numSamples,
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
