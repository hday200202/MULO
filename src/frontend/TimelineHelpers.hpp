#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>
#include <limits>
#include <cmath>
#include <juce_audio_formats/juce_audio_formats.h>
#include <unordered_map>

#include "Engine.hpp"
#include "UIData.hpp"

/**
 * @brief Generate visual rectangles for audio clips on the timeline.
 * 
 * Creates drawable rectangles representing audio clips positioned according to BPM and scroll offset.
 * Each clip includes both the background rectangle and embedded waveform visualization.
 * 
 * @param bpm Beats per minute for timeline positioning calculations
 * @param beatWidth Width in pixels of one beat
 * @param scrollOffset Horizontal scroll offset for the timeline view
 * @param rowSize Dimensions of the track row (width x height)
 * @param clips Vector of audio clips to render
 * @return Vector of drawable objects representing the clips and their waveforms
 */
inline std::vector<std::shared_ptr<sf::Drawable>> generateClipRects(
    double bpm, float beatWidth, float scrollOffset, const sf::Vector2f& rowSize, std::vector<AudioClip> clips
);

/**
 * @brief Create the playhead indicator at the current playback position.
 * 
 * Generates a vertical red line that shows the current playback position on the timeline.
 * 
 * @param bpm Beats per minute for position calculations
 * @param beatWidth Width in pixels of one beat
 * @param scrollOffset Horizontal scroll offset for the timeline view
 * @param seconds Current playback time in seconds
 * @param rowSize Dimensions of the track row (determines playhead height)
 * @return Drawable rectangle representing the playhead
 */
inline std::shared_ptr<sf::Drawable> getPlayHead(double bpm, float beatWidth, float scrollOffset, float seconds, const sf::Vector2f& rowSize);

/**
 * @brief Find the closest measure line X position to a given point.
 * 
 * Used for snapping functionality when placing or moving clips to align with measure boundaries.
 * 
 * @param pos Target position to find nearest measure line for
 * @param lines Vector of measure line drawables to search through
 * @return X coordinate of the closest measure line
 */
inline float getNearestMeasureX(const sf::Vector2f& pos, const std::vector<std::shared_ptr<sf::Drawable>>& lines);

/**
 * @brief Convert time in seconds to X pixel position on the timeline.
 * 
 * Calculates the horizontal pixel position for a given time based on BPM and beat width.
 * 
 * @param bpm Beats per minute for the timeline
 * @param beatWidth Width in pixels of one beat
 * @param seconds Time value to convert
 * @return X pixel position on the timeline
 */
inline float secondsToXPosition(double bpm, float beatWidth, float seconds);

/**
 * @brief Generate waveform visualization lines for an audio clip.
 * 
 * Creates vertical lines representing audio amplitude peaks across the duration of a clip.
 * Uses cached waveform data for performance.
 * 
 * @param clip Audio clip to generate waveform for
 * @param clipPosition Top-left position of the clip rectangle
 * @param clipSize Dimensions of the clip rectangle
 * @return Vector of drawable lines representing the waveform
 */
inline std::vector<std::shared_ptr<sf::Drawable>> generateWaveformData(const AudioClip& clip, const sf::Vector2f& clipPosition, const sf::Vector2f& clipSize);

static std::unordered_map<std::string, std::vector<float>> s_waveformCache;

/**
 * @brief Get reference to the global waveform cache.
 * 
 * Provides access to the static cache that stores pre-computed waveform peak data
 * for audio files to avoid repeated processing.
 * 
 * @return Reference to the waveform cache map (filepath -> peak values)
 */
inline std::unordered_map<std::string, std::vector<float>>& getWaveformCache() {
    static std::unordered_map<std::string, std::vector<float>> s_waveformCache;
    return s_waveformCache;
}

/**
 * @brief Ensure waveform data is cached for the given audio clip.
 * 
 * Reads audio file and generates peak data if not already cached. Uses JUCE audio
 * format reader to process the file and extract amplitude peaks for visualization.
 * 
 * @param clip Audio clip to ensure waveform data is cached for
 */
inline void ensureWaveformIsCached(const AudioClip& clip) {
    if (!clip.sourceFile.existsAsFile()) {
        return;
    }

    auto& cache = getWaveformCache();
    std::string filePath = clip.sourceFile.getFullPathName().toStdString();
    
    if (cache.count(filePath)) return;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(clip.sourceFile));

    if (!reader) {
        cache[filePath] = {};
        return;
    }

    long long totalSamples = reader->lengthInSamples;
    if (totalSamples == 0) {
        cache[filePath] = {};
        return;
    }
    
    const int desiredPeaks = std::floor(clip.duration * 50);
    long long samplesPerPeak = totalSamples / desiredPeaks;
    if (samplesPerPeak < 1) samplesPerPeak = 1;

    std::vector<float> peaks;
    peaks.reserve(desiredPeaks);

    juce::AudioBuffer<float> buffer(reader->numChannels, static_cast<int>(samplesPerPeak));

    for (int i = 0; i < desiredPeaks; ++i) {
        long long startSample = i * samplesPerPeak;
        if (startSample >= totalSamples) break;

        int numSamplesToRead = std::min((int)samplesPerPeak, (int)(totalSamples - startSample));
        reader->read(&buffer, 0, numSamplesToRead, startSample, true, true);

        float maxAmplitude = 0.0f;
        for (int channel = 0; channel < reader->numChannels; ++channel) {
            maxAmplitude = std::max(maxAmplitude, buffer.getMagnitude(0, numSamplesToRead));
        }
        peaks.push_back(maxAmplitude);
    }

    cache[filePath] = peaks;
}

/**
 * @brief Generate measure and beat lines for the timeline grid.
 * 
 * Creates vertical lines marking measures and beats according to the time signature.
 * Only generates lines that are visible within the current viewport for performance.
 * 
 * @param measureWidth Width in pixels of one complete measure
 * @param scrollOffset Horizontal scroll offset for the timeline view
 * @param rowSize Dimensions of the track row (determines line height)
 * @param sigNumerator Time signature numerator (beats per measure)
 * @param sigDenominator Time signature denominator (note value that gets the beat)
 * @return Vector of drawable lines representing measures and beats
 */
inline std::vector<std::shared_ptr<sf::Drawable>> generateTimelineMeasures(
    float measureWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize,
    unsigned int sigNumerator = 4,
    unsigned int sigDenominator = 4
) {
    std::vector<std::shared_ptr<sf::Drawable>> lines;
    
    // Calculate how many measures we need to draw
    float visibleWidth = rowSize.x;
    float startX = -scrollOffset;
    float endX = startX + visibleWidth;
    
    int startMeasure = static_cast<int>(std::floor(startX / measureWidth));
    int endMeasure = static_cast<int>(std::ceil(endX / measureWidth)) + 1;
    
    // Generate measure lines
    for (int measure = startMeasure; measure <= endMeasure; ++measure) {
        float xPos = measure * measureWidth + scrollOffset;
        
        if (xPos >= -10 && xPos <= visibleWidth + 10) {
            auto measureLine = std::make_shared<sf::RectangleShape>();
            measureLine->setSize({2.f, rowSize.y});
            measureLine->setPosition({xPos, 0.f});
            measureLine->setFillColor(line_color);
            lines.push_back(measureLine);
        }
        
        // Generate sub-measure lines (beats)
        float beatWidth = measureWidth / sigNumerator;
        for (unsigned int beat = 1; beat < sigNumerator; ++beat) {
            float beatX = xPos + (beat * beatWidth);
            
            if (beatX >= -10 && beatX <= visibleWidth + 10) {
                auto subMeasureLine = std::make_shared<sf::RectangleShape>();
                subMeasureLine->setSize({1.f, rowSize.y});
                subMeasureLine->setPosition({beatX, 0.f});
                sf::Color transparentLineColor = line_color;
                transparentLineColor.a = 100;
                subMeasureLine->setFillColor(transparentLineColor);
                lines.push_back(subMeasureLine);
            }
        }
    }
    
    return lines;
}

/**
 * @brief Generate visual rectangles for audio clips on the timeline with waveforms.
 * 
 * Creates the main implementation for clip visualization. Each clip gets a background
 * rectangle and overlaid waveform data. Positioning is calculated based on BPM timing.
 * 
 * @param bpm Beats per minute for timeline positioning calculations
 * @param beatWidth Width in pixels of one beat
 * @param scrollOffset Horizontal scroll offset for the timeline view
 * @param rowSize Dimensions of the track row (width x height)
 * @param clips Vector of audio clips to render
 * @return Vector of drawable objects (clip backgrounds + waveform lines)
 */
inline std::vector<std::shared_ptr<sf::Drawable>> generateClipRects(
    double bpm, float beatWidth, float scrollOffset, const sf::Vector2f& rowSize, std::vector<AudioClip> clips
) {
    std::vector<std::shared_ptr<sf::Drawable>> clipRects;

    float pixelsPerSecond = (beatWidth * bpm) / 60.0f;

    for (auto& ac : clips) {
        auto clipRect = std::make_shared<sf::RectangleShape>();
        float clipWidthPixels = ac.duration * pixelsPerSecond;
        float clipXPosition = (ac.startTime * pixelsPerSecond) + scrollOffset;
        
        clipRect->setSize({clipWidthPixels, rowSize.y});
        clipRect->setPosition({clipXPosition, 0.f});
        clipRect->setFillColor(clip_color);
        
        clipRects.push_back(clipRect);

        auto waveformDrawables = generateWaveformData(
            ac, 
            sf::Vector2f(clipXPosition, 0.f), 
            sf::Vector2f(clipWidthPixels, rowSize.y)
        );
        
        clipRects.insert(clipRects.end(), waveformDrawables.begin(), waveformDrawables.end());
    }
    
    return clipRects;
}

/**
 * @brief Create the playhead indicator at the current playback position.
 * 
 * Implementation that generates a semi-transparent red vertical line showing
 * the current playback position on the timeline.
 * 
 * @param bmp Beats per minute for position calculations
 * @param beatWidth Width in pixels of one beat
 * @param scrollOffset Horizontal scroll offset for the timeline view
 * @param seconds Current playback time in seconds
 * @param rowSize Dimensions of the track row (determines playhead height)
 * @return Drawable rectangle representing the playhead
 */
inline std::shared_ptr<sf::Drawable> getPlayHead(double bpm, float beatWidth, float scrollOffset, float seconds, const sf::Vector2f& rowSize) {
    auto playHeadRect = std::make_shared<sf::RectangleShape>();

    float xPosition = secondsToXPosition(bpm, beatWidth, seconds);

    playHeadRect->setSize({4.f, rowSize.y});
    playHeadRect->setPosition({xPosition + scrollOffset, 0.f});
    playHeadRect->setFillColor(sf::Color(255, 0, 0, 100));
    
    return playHeadRect;
}

/**
 * @brief Find the closest measure line X position to a given point.
 * 
 * Implementation for snapping functionality. Searches through measure lines
 * to find the one with minimum distance to the target position.
 * 
 * @param pos Target position to find nearest measure line for
 * @param lines Vector of measure line drawables to search through
 * @return X coordinate of the closest measure line, or pos.x if no lines found
 */
inline float getNearestMeasureX(const sf::Vector2f& pos, const std::vector<std::shared_ptr<sf::Drawable>>& lines) {
    if (lines.empty()) return pos.x;
    
    float closestX = 0.0f;
    float minDistance = std::numeric_limits<float>::max();
    
    for (const auto& line : lines) {
        if (auto rect = std::dynamic_pointer_cast<sf::RectangleShape>(line)) {
            float lineX = rect->getPosition().x;
            float distance = std::abs(pos.x - lineX);
            
            if (distance < minDistance) {
                minDistance = distance;
                closestX = lineX;
            }
        }
    }
    
    return closestX;
}

/**
 * @brief Convert time in seconds to X pixel position on the timeline.
 * 
 * Core timing calculation that converts musical time to screen coordinates.
 * Uses BPM to determine pixels per second rate.
 * 
 * @param bpm Beats per minute for the timeline
 * @param beatWidth Width in pixels of one beat
 * @param seconds Time value to convert
 * @return X pixel position on the timeline
 */
inline float secondsToXPosition(double bpm, float beatWidth, float seconds) {
    float pixelsPerSecond = (beatWidth * bpm) / 60.0f;
    return seconds * pixelsPerSecond;
}

/**
 * @brief Convert X pixel position to time in seconds.
 * 
 * Inverse operation of secondsToXPosition. Converts screen coordinates
 * back to musical time for interaction handling.
 * 
 * @param bpm Beats per minute for the timeline
 * @param beatWidth Width in pixels of one beat
 * @param xPos X pixel position to convert
 * @param scrollOffset Horizontal scroll offset (currently unused in calculation)
 * @return Time in seconds corresponding to the pixel position
 */
inline float xPosToSeconds(double bpm, float beatWidth, float xPos, float scrollOffset) {
    float pixelsPerSecond = (beatWidth * bpm) / 60.0f;
    return (xPos) / pixelsPerSecond;
}

/**
 * @brief Generate waveform visualization lines for an audio clip.
 * 
 * Implementation that creates vertical lines representing audio amplitude peaks.
 * Uses cached peak data and scales the visualization to fit within the clip rectangle.
 * Only draws peaks above a minimum threshold for visual clarity.
 * 
 * @param clip Audio clip to generate waveform for
 * @param clipPosition Top-left position of the clip rectangle
 * @param clipSize Dimensions of the clip rectangle
 * @return Vector of drawable lines representing the waveform peaks
 */
inline std::vector<std::shared_ptr<sf::Drawable>> generateWaveformData(const AudioClip& clip, const sf::Vector2f& clipPosition, const sf::Vector2f& clipSize) {
    ensureWaveformIsCached(clip);

    auto& cache = getWaveformCache();
    std::string filePath = clip.sourceFile.getFullPathName().toStdString();
    const auto& peaks = cache.at(filePath);

    std::vector<std::shared_ptr<sf::Drawable>> waveformDrawables;
    if (peaks.empty() || clipSize.x <= 0) return waveformDrawables;

    const int numPeaks = peaks.size();
    const float lineWidth = 4.f;
    waveformDrawables.reserve(numPeaks);

    for (int i = 0; i < numPeaks; ++i) {
        float peakValue = peaks[i];
        if (peakValue > 0.001f) {
            auto waveformLine = std::make_shared<sf::RectangleShape>();
            
            float lineHeight = peakValue * (clipSize.y * 0.9f);
            float lineX = clipPosition.x + (static_cast<float>(i) / numPeaks) * clipSize.x;
            float lineY = clipPosition.y + (clipSize.y - lineHeight) / 2.0f;
            
            waveformLine->setSize({lineWidth, lineHeight});
            waveformLine->setPosition({lineX, lineY});
            sf::Color waveformColorWithAlpha = waveform_color;
            waveformColorWithAlpha.a = 180;
            waveformLine->setFillColor(waveformColorWithAlpha);
            
            waveformDrawables.push_back(waveformLine);
        }
    }
    
    return waveformDrawables;
}