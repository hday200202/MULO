// EngineInterface.cpp
// Engine-related member function implementations for Application class

#include "Application.hpp"
#include "../audio/MIDIClip.hpp"
#include "tinyfiledialogs/tinyfiledialogs.hpp"
#include <thread> 

Track* Application::getMasterTrack() { 
    return engine.getMasterTrack(); 
}

Track* Application::getTrack(const std::string& name) { 
    return engine.getTrackByName(name); 
}

std::vector<std::unique_ptr<Track>>& Application::getAllTracks() { 
    return engine.getAllTracks(); 
}

void Application::addTrack(const std::string& name, const std::string& samplePath) { 
    engine.addTrack(name, samplePath); 
}

void Application::removeTrack(const std::string& name) { 
    pendingTrackRemoveName = name; 
}

void Application::exportAudio() {
    const char* path = tinyfd_selectFolderDialog("Select Export Directory", uiState.getExecutableDirectory().c_str());
    if (path) {
        std::string compositionName = engine.getCurrentCompositionName();
        DEBUG_PRINT("Export: Composition name from engine: '" << compositionName << "'");
        if (compositionName.empty()) {
            compositionName = "untitled";
        }
        // Ensure .wav extension
        if (compositionName.find(".wav") == std::string::npos) {
            compositionName += ".wav";
        }
        std::string fullPath = std::string(path) + "/" + compositionName;
        DEBUG_PRINT("Export: Selected directory: " << path);
        DEBUG_PRINT("Export: Full path to export: " << fullPath);
        engine.exportMaster(fullPath);
    }
}

void Application::setMetronomeEnabled(bool enabled) { 
    engine.setMetronomeEnabled(enabled); 
}

bool Application::isMetronomeEnabled() const { 
    return engine.isMetronomeEnabled(); 
}

void Application::playSound(const std::string& filePath, float db) { 
    // Run in a separate thread to avoid blocking the UI
    std::thread([this, filePath, db]() {
        engine.playSound(filePath, db);
    }).detach();
}

void Application::playSound(const juce::File& file, float db) { 
    // Run in a separate thread to avoid blocking the UI
    std::thread([this, file, db]() {
        engine.playSound(file, db);
    }).detach();
}

std::string Application::getEngineStateString() const { 
    return engine.getStateString(); 
}

void Application::loadEngineStateString(const std::string& stateString) { 
    engine.load(stateString); 
}

std::string Application::getEngineStateHash() const { 
    return engine.getStateHash(); 
}

void Application::sendMIDINote(int noteNumber, int velocity, bool noteOn) {
    engine.sendRealtimeMIDI(noteNumber, velocity, noteOn);
}

void Application::addEffect(const std::string& filePath) {
    pendingEffectPath = filePath;
    hasPendingEffect = true;
}

void Application::addSynthesizer(const std::string& filePath) {
    pendingSynthPath = filePath;
    hasPendingSynth = true;
}

void Application::requestOpenEffectWindow(size_t effectIndex) {
    pendingEffectWindowIndex = effectIndex;
    hasPendingEffectWindow = true;
}

void Application::deferEffectLoading(const std::string& trackName, const std::string& vstPath, bool openWindow, bool enabled, int index, const std::vector<std::pair<int, float>>& parameters) {
    DeferredEffect def;
    def.trackName = trackName;
    def.vstPath = vstPath;
    def.shouldOpenWindow = openWindow;
    def.enabled = enabled;
    def.index = index;
    def.parameters = parameters;
    
    deferredEffects.push_back(def);
    hasDeferredEffects = true;
}

void Application::play() { 
    engine.play(); 
}

void Application::pause() { 
    engine.pause(); 
}

void Application::setSavedPosition(double seconds) { 
    engine.setPosition(seconds); 
}

bool Application::isPlaying() const { 
    return engine.isPlaying(); 
}

void Application::setBpm(float bpm) { 
    engine.setBpm(bpm); 
}

float Application::getBpm() const { 
    return engine.getBpm(); 
}

double Application::getPosition() const { 
    return engine.getPosition(); 
}

double Application::getSavedPosition() const { 
    return engine.getSavedPosition(); 
}

void Application::setPosition(double seconds) { 
    engine.setPosition(seconds); 
}

std::pair<int, int> Application::getTimeSignature() {
    return engine.getTimeSignature(); 
}

AudioClip* Application::getReferenceClip(const std::string& trackName) { 
    return engine.getTrackByName(trackName)->getReferenceClip(); 
}

void Application::addClipToTrack(const std::string& trackName, const AudioClip& clip) { 
    engine.getTrackByName(trackName)->addClip(clip); 
    std::string currentRoom = readConfig<std::string>("collab_room", "");
    if (!currentRoom.empty())
        updateRoomEngineState(currentRoom, engine.getStateString());
}

void Application::removeClipFromTrack(const std::string& trackName, size_t index) { 
    engine.getTrackByName(trackName)->removeClip(index); 
    std::string currentRoom = readConfig<std::string>("collab_room", "");
    if (!currentRoom.empty())
        updateRoomEngineState(currentRoom, engine.getStateString());
}

void Application::updateClipInTrack(const std::string& trackName, size_t index, const AudioClip& newClip) {
    auto* track = engine.getTrackByName(trackName);
    if (track && index < track->getClips().size()) {
        track->removeClip(index);
        track->addClip(newClip);
        std::string currentRoom = readConfig<std::string>("collab_room", "");
        if (!currentRoom.empty())
            updateRoomEngineState(currentRoom, engine.getStateString());
    }
}

double Application::getSampleRate() const { 
    return engine.getSampleRate(); 
}

void Application::setSampleRate(const double newSampleRate) { 
    uiState.sampleRate = newSampleRate;
    writeConfig("sampleRate", newSampleRate);
    engine.configureAudioDevice(newSampleRate);
}

void Application::setSelectedTrack(const std::string& trackName) { 
    engine.setSelectedTrack(trackName); 
}

std::string Application::getSelectedTrack() const { 
    return engine.getSelectedTrack(); 
}

Track* Application::getSelectedTrackPtr() { 
    return engine.getSelectedTrackPtr(); 
}

bool Application::hasSelectedTrack() const { 
    return engine.hasSelectedTrack(); 
}

void Application::loadComposition(const std::string& path) { 
    engine.loadComposition(path); 
    engine.generateMetronomeTrack(); 
}

std::string Application::getCurrentCompositionName() const { 
    return engine.getCurrentCompositionName(); 
}

void Application::setCurrentCompositionName(const std::string& name) { 
    engine.setCurrentCompositionName(name); 
}

void Application::saveState() { 
    engine.save(); 
}

void Application::saveToFile(const std::string& path) const { 
    engine.save(path); 
}

MIDIClip* Application::getSelectedMIDIClip() const {
    std::string selectedTrackName = getSelectedTrack();
    if (selectedTrackName.empty()) return nullptr;
    
    Track* selectedTrack = const_cast<Application*>(this)->getTrack(selectedTrackName);
    if (selectedTrack && selectedTrack->getType() == Track::TrackType::MIDI) {
        MIDITrack* midiTrack = static_cast<MIDITrack*>(selectedTrack);
        const auto& midiClips = midiTrack->getMIDIClips();
        
        if (!midiClips.empty())
            return const_cast<MIDIClip*>(&midiClips[0]);
    }
    
    return nullptr;
}

MIDIClip* Application::getTimelineSelectedMIDIClip() const {
    if (auto* timelineComponent = const_cast<Application*>(this)->getComponent("timeline"))
        return timelineComponent->getSelectedMIDIClip();
    return nullptr;
}

void Application::updateParameterTracking() {
    for (auto& track : getAllTracks())
        if (track) track->updateParameterTracking();
    
    if (auto* masterTrack = getMasterTrack())
        masterTrack->updateParameterTracking();
}
