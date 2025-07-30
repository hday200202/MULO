#pragma once

#include <string>
#include <vector>
#include <memory>

#include "Track.hpp"

/**
 * @brief Represents a musical composition/project.
 */
struct Composition {
    std::string name = "untitled";
    double bpm = 120;
    int timeSigNumerator = 4;
    int timeSigDenominator = 4;

    std::vector<std::unique_ptr<Track>> tracks; // Changed to unique_ptr

    Composition();
    Composition(const std::string& compositionPath);
    ~Composition();
};