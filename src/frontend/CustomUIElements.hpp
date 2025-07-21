#pragma once

#include <SFML/Graphics.hpp>
#include <vector>
#include <memory>

class TimelineMeasures {
public:
    TimelineMeasures(unsigned int sigNumerator = 4, unsigned int sigDenominator = 4);

    std::vector<std::shared_ptr<sf::Drawable>> generateLines(
        float measureWidth,
        float scrollOffset,
        const sf::Vector2f& rowSize
    );

private:
    unsigned int sigNumerator;
    unsigned int sigDenominator;

    sf::RectangleShape measureLine;
    sf::RectangleShape subMeasureLine;
};