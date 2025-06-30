#include "Engine.hpp"

Engine::Engine()
{
}

Engine::Engine(std::unique_ptr<Composition> newComposition)
{
}

Engine::~Engine()
{
}

void Engine::loadComposition(const std::string &compositionPath)
{
}

void Engine::addTrack(const std::string &trackName)
{
}

void Engine::removeTrack(const std::string &trackName)
{
}

std::unique_ptr<Track> Engine::getTrack(const std::string &trackName)
{
    return std::unique_ptr<Track>();
}

std::vector<std::unique_ptr<Track>> Engine::getAllTracks()
{
    return std::vector<std::unique_ptr<Track>>();
}

Composition::Composition()
{
}

Composition::Composition(const std::string &compositionPath)
{
}

Composition::~Composition()
{
}

Track::Track()
{
}

Track::~Track()
{
}

void Track::setVolume(const float db)
{
}

void Track::setPan(const float pan)
{
}

void Track::setTrackIndex(const int newIndex)
{
}

void Track::toggleMute()
{
}

void Track::toggleSolo()
{
}

void Track::addClip(const AudioClip &newClip)
{
}
