#include "CustomUIElements.hpp"
#include <cmath>

TimelineMeasures::TimelineMeasures(const unsigned int sigNum, const unsigned int sigDen)
    : sigNumerator(sigNum), sigDenominator(sigDen) {
    measureLine.setFillColor(sf::Color(50, 50, 50, 150));
    subMeasureLine.setFillColor(sf::Color(50, 50, 50, 100));
}

std::vector<std::shared_ptr<sf::Drawable>> TimelineMeasures::generateLines(
    float measureWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize
) {
    std::vector<std::shared_ptr<sf::Drawable>> allLines;

    if (measureWidth <= 0 || sigNumerator <= 0) {
        return allLines;
    }

    measureLine.setSize({2.f, rowSize.y});
    subMeasureLine.setSize({1.f, rowSize.y});

    float startPos = fmod(fmod(scrollOffset, measureWidth) + measureWidth, measureWidth);
    const float beatWidth = measureWidth / sigNumerator;

    for (float pos = startPos; pos < rowSize.x; pos += measureWidth) {
        auto line = std::make_shared<sf::RectangleShape>(measureLine);
        line->setPosition({pos, 0});
        allLines.push_back(line);
    }
    for (float pos = startPos - measureWidth; pos > -measureWidth; pos -= measureWidth) {
        auto line = std::make_shared<sf::RectangleShape>(measureLine);
        line->setPosition({pos, 0});
        allLines.push_back(line);
    }

    float subStartPos = fmod(fmod(scrollOffset, beatWidth) + beatWidth, beatWidth);
    for (float subPos = subStartPos; subPos < rowSize.x; subPos += beatWidth) {
        auto line = std::make_shared<sf::RectangleShape>(subMeasureLine);
        line->setPosition({subPos, 0});
        allLines.push_back(line);
    }
    for (float subPos = subStartPos - beatWidth; subPos > -beatWidth; subPos -= beatWidth) {
        auto line = std::make_shared<sf::RectangleShape>(subMeasureLine);
        line->setPosition({subPos, 0});
        allLines.push_back(line);
    }

    return allLines;
}