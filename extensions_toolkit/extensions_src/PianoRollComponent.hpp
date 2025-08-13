#pragma once

#include "MULOComponent.hpp"
#include "../../src/DebugConfig.hpp"
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <iostream>

inline std::vector<std::shared_ptr<sf::Drawable>> generatePianoRollMeasures(
    float measureWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize,
    unsigned int sigNumerator,
    unsigned int sigDenominator,
    sf::Color lineColor
);

class PianoRoll : public MULOComponent {
public:
    PianoRoll();
    ~PianoRoll() override;

    void init() override;
    void update() override;
    bool handleEvents() override;
    
    void show() override { setPianoRollVisible(true); }
    void hide() override { setPianoRollVisible(false); }
    bool isVisible() const override { return pianoRollShown; }
    void setPianoRollVisible(bool visible);

private:
    static constexpr int TOTAL_NOTES = 120;
    std::string notes[12] = {
        "B", "A#", "A", "G#", "G", "F#", "F", "E", "D#", "D", "C#", "C"
    };

    int keyboardOctave = 4;

    std::unordered_map<std::string, sf::Keyboard::Key> noteKeys = {
        {"C", sf::Keyboard::Key::A},
        {"C#", sf::Keyboard::Key::W},
        {"D", sf::Keyboard::Key::S},
        {"D#", sf::Keyboard::Key::E},
        {"E", sf::Keyboard::Key::D},
        {"F", sf::Keyboard::Key::F},
        {"F#", sf::Keyboard::Key::T},
        {"G", sf::Keyboard::Key::G},
        {"G#", sf::Keyboard::Key::Y},
        {"A", sf::Keyboard::Key::H},
        {"A#", sf::Keyboard::Key::U},
        {"B", sf::Keyboard::Key::J}
    };

    ScrollableColumn* baseColumn;
    float pianoRollOffset = 0.f;
    std::unordered_map<std::string, ScrollableRow*> noteRows;
    bool pianoRollShown = false;
    bool wasVisible = false;

    std::vector<std::shared_ptr<sf::Drawable>> cachedMeasureLines;
    float lastMeasureWidth = -1.f;
    float lastScrollOffset = -1.f;
    sf::Vector2f lastRowSize = {-1.f, -1.f};

    Row* noteRow(const std::string& note);
    void handleScrollSynchronization();
    void handleMeasureLines();
    const std::vector<std::shared_ptr<sf::Drawable>>& getCachedMeasureLines(
        float measureWidth, 
        float scrollOffset, 
        const sf::Vector2f& rowSize
    );
};

inline std::vector<std::shared_ptr<sf::Drawable>> generatePianoRollMeasures(
    float measureWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize,
    unsigned int sigNumerator,
    unsigned int sigDenominator,
    sf::Color lineColor
) {
    std::vector<std::shared_ptr<sf::Drawable>> measures;
    
    const float startX = -scrollOffset;
    const float endX = rowSize.x - scrollOffset;
    const float beatWidth = measureWidth;
    const float subBeatWidth = beatWidth / 4.0f;
    
    const int startBeat = static_cast<int>(std::floor(startX / beatWidth));
    const int endBeat = static_cast<int>(std::ceil(endX / beatWidth)) + 1;

    for (int i = startBeat; i <= endBeat; ++i) {
        const float x = i * beatWidth + scrollOffset;
        
        if (x >= -beatWidth && x <= rowSize.x + beatWidth) {
            auto line = std::make_shared<sf::RectangleShape>(sf::Vector2f(2.f, rowSize.y));
            line->setPosition(sf::Vector2f(x, 0.f));
            line->setFillColor(lineColor);
            measures.push_back(line);
        }
    }
    
    const int startSubBeat = static_cast<int>(std::floor(startX / subBeatWidth));
    const int endSubBeat = static_cast<int>(std::ceil(endX / subBeatWidth)) + 1;

    for (int i = startSubBeat; i <= endSubBeat; ++i) {
        const float x = i * subBeatWidth + scrollOffset;
        
        if (fmod(i * subBeatWidth, beatWidth) < 0.1f) continue;
        
        if (x >= -subBeatWidth && x <= rowSize.x + subBeatWidth) {
            auto line = std::make_shared<sf::RectangleShape>(sf::Vector2f(1.f, rowSize.y));
            line->setPosition(sf::Vector2f(x, 0.f));
            line->setFillColor(lineColor);
            measures.push_back(line);
        }
    }

    return measures;
}

#include "Application.hpp"

PianoRoll::PianoRoll() { 
    name = "piano_roll"; 
}

PianoRoll::~PianoRoll() {}

void PianoRoll::setPianoRollVisible(bool visible) {
    pianoRollShown = visible;
    forceUpdate = true;
}

void PianoRoll::init() {
    if (app->mainContentRow)
        parentContainer = app->mainContentRow;
    
    baseColumn = scrollableColumn(
        Modifier().setWidth(1.f).setHeight(1.f).align(Align::RIGHT)
                  .setColor(app->resources.activeTheme->middle_color),
        contains{}
    );

    for (int i = 0; i < TOTAL_NOTES; ++i) {
        std::string note = notes[i % 12] + std::to_string(10 - (i / 12));
        baseColumn->addElements({
            noteRow(note), 
            spacer(Modifier().setfixedHeight(3))
        });
    }

    layout = baseColumn;

    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }

    hide();
}

void PianoRoll::update() {
    if (pianoRollShown && !wasVisible) {
        if (auto* mixer = app->getComponent("mixer")) {
            if (mixer->getLayout()) {
                mixer->getLayout()->m_modifier.setVisible(false);
                mixer->getLayout()->m_modifier.setWidth(0.f);
            }
            mixer->hide();
        }
        wasVisible = true;
    }
    else if (!pianoRollShown && wasVisible) {
        wasVisible = false;
    }

    if (auto* timelineComponent = app->getComponent("timeline")) {
        if (timelineComponent->getLayout()) {
            if (pianoRollShown) {
                timelineComponent->getLayout()->m_modifier.setVisible(false);
                timelineComponent->getLayout()->m_modifier.setWidth(0.f);
                timelineComponent->hide();
            } else {
                if (auto* mixer = app->getComponent("mixer")) {
                    if (!mixer->isVisible()) {
                        timelineComponent->getLayout()->m_modifier.setVisible(true);
                        timelineComponent->getLayout()->m_modifier.setWidth(1.f);
                        timelineComponent->show();
                    }
                }
            }
        }
    }

    if (layout && pianoRollShown) {
        layout->m_modifier.setVisible(true);
        layout->m_modifier.setWidth(1.f);
    }
    else if (layout && !pianoRollShown) {
        layout->m_modifier.setVisible(false);
        return;
    }

    baseColumn->m_modifier.setWidth(1.f);
    handleScrollSynchronization();
    handleMeasureLines();
}

void PianoRoll::handleMeasureLines() {
    if (!isVisible()) return;

    const double bpm = app->getBpm();
    const float zoomLevel = app->uiState.timelineZoomLevel;
    const float beatWidth = 100.f * zoomLevel;

    for (const auto& [noteName, scrollableRow] : noteRows) {
        if (!scrollableRow) continue;

        const sf::Vector2f trackRowSize = scrollableRow->getSize();
        auto measureLines = getCachedMeasureLines(beatWidth, pianoRollOffset, trackRowSize);
        scrollableRow->setCustomGeometry(measureLines);
    }
}

const std::vector<std::shared_ptr<sf::Drawable>>& PianoRoll::getCachedMeasureLines(
    float measureWidth, 
    float scrollOffset, 
    const sf::Vector2f& rowSize
) {
    bool shouldRebuild = (measureWidth != lastMeasureWidth) || 
                        (scrollOffset != lastScrollOffset) || 
                        (rowSize != lastRowSize);

    if (shouldRebuild) {
        cachedMeasureLines = generatePianoRollMeasures(
            measureWidth, scrollOffset, rowSize, 4, 4, 
            app->resources.activeTheme->line_color
        );
        lastMeasureWidth = measureWidth;
        lastScrollOffset = scrollOffset;
        lastRowSize = rowSize;
    }

    return cachedMeasureLines;
}

bool PianoRoll::handleEvents() {
    return false;
}

Row* PianoRoll::noteRow(const std::string& note) {
    auto* scrollableRowElement = scrollableRow(
        Modifier().align(Align::LEFT).setColor(app->resources.activeTheme->middle_color),
        contains{}
    );
    
    noteRows[note] = scrollableRowElement;
    bool isSharp = note.find("#") != std::string::npos;
    
    return row(
        Modifier().setfixedHeight(32).align(Align::LEFT),
        contains{
            scrollableRowElement,
            row(
                Modifier()
                    .setfixedWidth(128)
                    .setColor(isSharp ? sf::Color::Black : sf::Color::White)
                    .align(Align::RIGHT),
                contains{}
            ),
        }
    );
}

void PianoRoll::handleScrollSynchronization() {
    float newOffset = pianoRollOffset;
    
    for (const auto& [noteName, scrollableRow] : noteRows) {
        if (scrollableRow && std::abs(scrollableRow->getOffset() - pianoRollOffset) > 0.1f) {
            newOffset = scrollableRow->getOffset();
            break;
        }
    }
    
    bool wasClamped = false;
    if (newOffset > 0.0f) {
        newOffset = 0.0f;
        wasClamped = true;
    }
    
    if (std::abs(newOffset - pianoRollOffset) > 0.1f || wasClamped) {
        pianoRollOffset = newOffset;
        
        for (const auto& [noteName, scrollableRow] : noteRows) {
            if (scrollableRow) {
                scrollableRow->setOffset(pianoRollOffset);
            }
        }
    }
}

GET_INTERFACE
DECLARE_PLUGIN(PianoRoll)
