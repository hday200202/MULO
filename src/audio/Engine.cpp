#include "Engine.hpp"
#include <fstream>
#include <iomanip>

//==============================================================================
// ENGINE 
//==============================================================================

Engine::Engine() {
    formatManager.registerBasicFormats();
    deviceManager.initialise(0, 2, nullptr, true);
    deviceManager.addAudioCallback(this);
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

void Engine::loadComposition(const std::string&) {}

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
    std::ofstream out(path);
    if (!out.is_open()) return false;

    out << "{\n";
    out << "  \"composition\": {\n";
    out << "    \"name\": \"" << escapeMpfString(currentComposition->name) << "\",\n";
    out << "    \"bpm\": " << currentComposition->bpm << ",\n";
    out << "    \"timeSignature\": {\n";
    out << "      \"numerator\": " << currentComposition->timeSigNumerator << ",\n";
    out << "      \"denominator\": " << currentComposition->timeSigDenominator << "\n";
    out << "    },\n";
    out << "    \"tracks\": [\n";
    for (size_t i = 0; i < currentComposition->tracks.size(); ++i) {
        const auto* track = currentComposition->tracks[i];
        out << "      {\n";
        out << "        \"name\": \"" << escapeMpfString(track->getName()) << "\",\n";
        out << "        \"volume\": " << track->getVolume() << ",\n";
        out << "        \"pan\": " << track->getPan() << ",\n";
        out << "        \"clips\": [\n";
        const auto& clips = track->getClips();
        for (size_t j = 0; j < clips.size(); ++j) {
            const auto& clip = clips[j];
            out << "          {\n";
            out << "            \"file\": \"" << escapeMpfString(clip.sourceFile.getFullPathName().toStdString()) << "\",\n";
            out << "            \"start\": " << clip.startTime << ",\n";
            out << "            \"offset\": " << clip.offset << ",\n";
            out << "            \"duration\": " << clip.duration << ",\n";
            out << "            \"volume\": " << clip.volume << "\n";
            out << "          }" << (j + 1 < clips.size() ? "," : "") << "\n";
        }
        out << "        ]\n";
        out << "      }" << (i + 1 < currentComposition->tracks.size() ? "," : "") << "\n";
    }
    out << "    ]\n";
    out << "  }\n";
    out << "}\n";

    out.close();
    return true;
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

void Engine::processBlock(juce::AudioBuffer<float>& output, int numSamples) {
    if (!currentComposition)
        return;

    for (auto* track : currentComposition->tracks) {
        track->process(positionSeconds, output, numSamples, sampleRate);
    }
}
bool Engine::loadState(const std::string& filePath) {
    juce::File file(filePath);
    if (!file.existsAsFile())
        return false;

    juce::var jsonVar = juce::JSON::parse(file);
    if (jsonVar.isVoid() || !jsonVar.isObject())
        return false;

    auto* root = jsonVar.getDynamicObject();
    if (!root)
        return false;

    std::string name = root->getProperty("name").toString().toStdString();
    newComposition(name);

    auto tracksVar = root->getProperty("tracks");
    if (!tracksVar.isArray())
        return false;

    for (const auto& t : *tracksVar.getArray()) {
        auto* trackObj = t.getDynamicObject();
        if (!trackObj)
            continue;

        std::string trackName = trackObj->getProperty("name").toString().toStdString();
        float volume = static_cast<float>(trackObj->getProperty("volume"));
        float pan = static_cast<float>(trackObj->getProperty("pan"));

        addTrack(trackName);
        Track* track = getTrack(currentComposition->tracks.size() - 1);
        if (track) {
            track->setVolume(volume);
            track->setPan(pan);
        }

        auto clipsVar = trackObj->getProperty("clips");
        if (clipsVar.isArray()) {
            for (const auto& c : *clipsVar.getArray()) {
                auto* clipObj = c.getDynamicObject();
                if (!clipObj)
                    continue;

                juce::File clipFile(clipObj->getProperty("sourceFile").toString());
                double start = static_cast<double>(clipObj->getProperty("startTime"));
                double offset = static_cast<double>(clipObj->getProperty("offset"));
                double duration = static_cast<double>(clipObj->getProperty("duration"));
                float clipVolume = static_cast<float>(clipObj->getProperty("volume"));

                AudioClip clip(clipFile, start, offset, duration, clipVolume);
                track->addClip(clip);
            }
        }
    }

    return true;
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
