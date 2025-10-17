#pragma once

#include "MULOComponent.hpp"
#include "../../src/DebugConfig.hpp"
#include "../../src/audio/MIDIClip.hpp"
#include "../../src/audio/MIDITrack.hpp"
#include "../../src/audio/Track.hpp"
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <iostream>

inline std::vector<std::shared_ptr<sf::Drawable>> generatePianoRollMeasures(
    float beatWidth,
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
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    int keyboardOctave = 4;

    std::unordered_map<std::string, sf::Keyboard::Key> noteKeys = {
        // First octave (C4-B4)
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
        {"B", sf::Keyboard::Key::J},
        // Next octave partial (C5-F5)
        {"C5", sf::Keyboard::Key::K},
        {"C#5", sf::Keyboard::Key::O},
        {"D5", sf::Keyboard::Key::L},
        {"D#5", sf::Keyboard::Key::P},
        {"E5", sf::Keyboard::Key::Semicolon},
        {"F5", sf::Keyboard::Key::Apostrophe}
    };

    ScrollableColumn* baseColumn;
    float pianoRollOffset = 0.f;
    float columnOffset = 0.f;
    std::unordered_map<std::string, ScrollableRow*> noteRows;
    bool pianoRollShown = false;
    bool wasVisible = false;

    std::vector<std::shared_ptr<sf::Drawable>> cachedMeasureLines;
    float lastMeasureWidth = -1.f;
    float lastScrollOffset = -1.f;
    sf::Vector2f lastRowSize = {-1.f, -1.f};
    float lastZoomLevel = -1.f;
    float pianoRollZoomLevel = 1.0f;

    // MIDI clip editing
    MIDIClip* selectedMIDIClip = nullptr;
    float clipDuration = 1.0f; // Default 1 second
    
    // Keyboard highlighting
    std::set<int> currentlyPressedNotes;
    std::unordered_map<int, Container*> pianoKeyElements; // Map MIDI note number to piano key element
    
    // Note extension dragging state
    bool isDraggingNote = false;
    int draggingNoteNumber = -1;
    float dragStartTime = 0.0f;

    Row* noteRow(const std::string& note, int midiNoteNumber);
    void handleScrollSynchronization();
    void handleMeasureLines();
    int calculateNoteNumber(const std::string& noteName, int octave) const;
    const std::vector<std::shared_ptr<sf::Drawable>>& getCachedMeasureLines(
        float beatWidth, 
        float scrollOffset, 
        const sf::Vector2f& rowSize
    );
    
    // MIDI clip editing methods
    void updateSelectedMIDIClip();
    void updateKeyboardHighlighting();
    void scrollToNote(int noteNumber);
    void handleNoteClick(int noteNumber, float xPosition, bool isRightClick = false);
    void handleNoteDragStart(int noteNumber, float xPosition);
    void handleNoteDragUpdate();
    void handleNoteDragEnd();
    float snapToGrid(float time) const;
    float getSubmeasureDuration() const;
    void deleteNoteAtTime(int noteNumber, float timelineTime);
    float xPositionToClipTime(float xPosition) const;
    int getNoteNumberFromRowName(const std::string& noteName) const;
    std::vector<std::shared_ptr<sf::Drawable>> generateNoteRectsForRow(int targetNoteNumber, const sf::Vector2f& rowSize) const;
    void handleZoom(float verticalDelta);
    float getCurrentPixelsPerSecond() const;
    void setInitialZoomForClip();
};

inline std::vector<std::shared_ptr<sf::Drawable>> generatePianoRollMeasures(
    float beatWidth,
    float scrollOffset,
    const sf::Vector2f& rowSize,
    unsigned int sigNumerator,
    unsigned int sigDenominator,
    sf::Color lineColor
) {
    std::vector<std::shared_ptr<sf::Drawable>> measures;
    
    const float startX = -scrollOffset;
    const float endX = rowSize.x - scrollOffset;
    const float submeasureWidth = beatWidth / 4.0f;
    
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
    
    const int startSubBeat = static_cast<int>(std::floor(startX / submeasureWidth));
    const int endSubBeat = static_cast<int>(std::ceil(endX / submeasureWidth)) + 1;

    for (int i = startSubBeat; i <= endSubBeat; ++i) {
        const float x = i * submeasureWidth + scrollOffset;
        
        if (fmod(i * submeasureWidth, beatWidth) < 0.1f) continue;
        
        if (x >= -submeasureWidth && x <= rowSize.x + submeasureWidth) {
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
    pianoRollZoomLevel = 1.0f;
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

    baseColumn->setScrollSpeed(40.f);

    for (int i = 0; i < TOTAL_NOTES; ++i) {
        int midiNoteNumber = 119 - i;
        std::string note = notes[midiNoteNumber % 12] + std::to_string(midiNoteNumber / 12 - 1);
        baseColumn->addElements({
            noteRow(note, midiNoteNumber), 
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
    updateSelectedMIDIClip();
    updateKeyboardHighlighting();
    handleNoteDragUpdate();
    
    if (pianoRollShown) {
        bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || 
                          sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
        
        if (ctrlPressed) {
            const float verticalDelta = app->ui->getVerticalScrollDelta();
            if (verticalDelta != 0.f) {
                handleZoom(verticalDelta);
                app->ui->resetScrollDeltas();
            }
        } else {
            if (baseColumn) {
                columnOffset = baseColumn->getOffset();
            }
        }
    }
    
    if (pianoRollShown && baseColumn) {
        baseColumn->setOffset(columnOffset);
    }
    
    if (pianoRollShown && !wasVisible) {
        if (baseColumn) {
            columnOffset = baseColumn->getOffset();
        }
        
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

    const float beatWidth = 100.f * pianoRollZoomLevel;
    const double bpm = app->getBpm();
    const float pixelsPerSecond = (beatWidth * bpm) / 60.0f;

    std::vector<std::shared_ptr<sf::Drawable>> baseCustomGeometry;
    
    if (selectedMIDIClip) {
        const sf::Vector2f baseSize = baseColumn->getSize();
        double currentEngineTime = app->getPosition();
        double clipStartTime = selectedMIDIClip->startTime;
        double clipEndTime = clipStartTime + clipDuration;
        
        if (currentEngineTime >= clipStartTime && currentEngineTime <= clipEndTime) {
            double clipRelativeTime = currentEngineTime - clipStartTime;
            float playheadX = static_cast<float>(clipRelativeTime * pixelsPerSecond) + pianoRollOffset;
            
            if (playheadX >= -10.0f && playheadX <= baseSize.x + 10.0f) {
                auto playhead = std::make_shared<sf::RectangleShape>(sf::Vector2f(3.0f, baseSize.y));
                playhead->setPosition(sf::Vector2f(playheadX, 0.0f));
                playhead->setFillColor(sf::Color::Red);
                baseCustomGeometry.push_back(playhead);
            }
        }
        
        float clipEndX = static_cast<float>(clipDuration * pixelsPerSecond) + pianoRollOffset;
        if (clipEndX >= -10.0f && clipEndX <= baseSize.x + 10.0f) {
            auto clipEndLine = std::make_shared<sf::RectangleShape>(sf::Vector2f(2.0f, baseSize.y));
            clipEndLine->setPosition(sf::Vector2f(clipEndX, 0.0f));
            clipEndLine->setFillColor(app->resources.activeTheme->clip_color);
            baseCustomGeometry.push_back(clipEndLine);
        }
    }
    
    baseColumn->setCustomGeometry(baseCustomGeometry);

    for (const auto& [noteName, scrollableRow] : noteRows) {
        if (!scrollableRow) continue;

        const sf::Vector2f trackRowSize = scrollableRow->getSize();
        auto measureLines = getCachedMeasureLines(beatWidth, pianoRollOffset, trackRowSize);
        
        if (selectedMIDIClip) {
            int noteNumber = getNoteNumberFromRowName(noteName);
            auto noteRectsForThisRow = generateNoteRectsForRow(noteNumber, trackRowSize);
            measureLines.insert(measureLines.end(), noteRectsForThisRow.begin(), noteRectsForThisRow.end());
        }
        
        scrollableRow->setCustomGeometry(measureLines);
    }
}

const std::vector<std::shared_ptr<sf::Drawable>>& PianoRoll::getCachedMeasureLines(
    float beatWidth, 
    float scrollOffset, 
    const sf::Vector2f& rowSize
) {
    float currentZoom = pianoRollZoomLevel;
    bool shouldRebuild = (beatWidth != lastMeasureWidth) || 
                        (scrollOffset != lastScrollOffset) || 
                        (rowSize != lastRowSize) ||
                        (currentZoom != lastZoomLevel);

    if (shouldRebuild) {
        auto [timeSigNum, timeSigDen] = app->getTimeSignature();
        cachedMeasureLines = generatePianoRollMeasures(
            beatWidth, scrollOffset, rowSize, timeSigNum, timeSigDen, 
            app->resources.activeTheme->line_color
        );
        lastMeasureWidth = beatWidth;
        lastScrollOffset = scrollOffset;
        lastRowSize = rowSize;
        lastZoomLevel = currentZoom;
    }

    return cachedMeasureLines;
}

bool PianoRoll::handleEvents() {
    static std::unordered_map<sf::Keyboard::Key, bool> keyStates;
    static bool zPressed = false;
    static bool xPressed = false;
    
    bool zCurrentlyPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z);
    bool xCurrentlyPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::X);
    
    if (zCurrentlyPressed && !zPressed) {
        keyboardOctave = std::max(-1, keyboardOctave - 1);
    }
    
    if (xCurrentlyPressed && !xPressed) {
        keyboardOctave = std::min(10, keyboardOctave + 1);
    }
    
    zPressed = zCurrentlyPressed;
    xPressed = xCurrentlyPressed;
    
    for (const auto& [noteName, key] : noteKeys) {
        bool keyPressed = sf::Keyboard::isKeyPressed(key);
        bool wasPressed = keyStates[key];
        
        if (keyPressed && !wasPressed) {
            int noteNumber;
            if (noteName.length() > 1 && noteName.back() == '5') {
                std::string baseNote = noteName.substr(0, noteName.length() - 1);
                noteNumber = calculateNoteNumber(baseNote, keyboardOctave + 1);
            } else {
                noteNumber = calculateNoteNumber(noteName, keyboardOctave);
            }
            currentlyPressedNotes.insert(noteNumber);
            
            // Only scroll to note if piano roll is visible and Left Control is held
            if (pianoRollShown && sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl)) {
                scrollToNote(noteNumber);
            }
            
            if (app) {
                app->sendMIDINote(noteNumber, 127, true);
            }
        } else if (!keyPressed && wasPressed) {
            int noteNumber;
            if (noteName.length() > 1 && noteName.back() == '5') {
                std::string baseNote = noteName.substr(0, noteName.length() - 1);
                noteNumber = calculateNoteNumber(baseNote, keyboardOctave + 1);
            } else {
                noteNumber = calculateNoteNumber(noteName, keyboardOctave);
            }
            currentlyPressedNotes.erase(noteNumber);
            if (app) {
                app->sendMIDINote(noteNumber, 127, false);
            }
        }
        
        keyStates[key] = keyPressed;
    }
    
    if (!pianoRollShown) {
        return false;
    }
    
    return false;
}

Row* PianoRoll::noteRow(const std::string& note, int midiNoteNumber) {
    bool isSharp = note.find("#") != std::string::npos;
    bool isC = note.substr(0, 1) == "C" && note.substr(0, 2) != "C#";
    
    auto* scrollableRowElement = scrollableRow(
        Modifier().align(Align::LEFT).setColor(isSharp ? app->resources.activeTheme->middle_color : app->resources.activeTheme->foreground_color)
                  .onLClick([this, note]() {
                      sf::Vector2f globalMousePos = app->ui->getMousePosition();
                      auto* scrollableRowElement = noteRows[note];
                      if (scrollableRowElement) {
                          sf::Vector2f localMousePos = globalMousePos - scrollableRowElement->getPosition();
                          int noteNum = getNoteNumberFromRowName(note);
                          handleNoteDragStart(noteNum, localMousePos.x);
                      }
                  })
                  .onRClick([this, note]() {
                      sf::Vector2f globalMousePos = app->ui->getMousePosition();
                      auto* scrollableRowElement = noteRows[note];
                      if (scrollableRowElement) {
                          sf::Vector2f localMousePos = globalMousePos - scrollableRowElement->getPosition();
                          int noteNum = getNoteNumberFromRowName(note);
                          handleNoteClick(noteNum, localMousePos.x, true);
                      }
                  }),
        contains{}
    );
    
    scrollableRowElement->setScrollSpeed(40.f);
    noteRows[note] = scrollableRowElement;
    
    Container* pianoKey;
    
    if (isC) {
        pianoKey = row(
            Modifier()
                .setfixedWidth(128)
                .setColor(sf::Color::White)
                .align(Align::RIGHT),
            contains{
                text(
                    Modifier()
                        .setColor(sf::Color::Black)
                        .align(Align::LEFT),
                    note,
                    app->resources.dejavuSansFont,
                    note + "_label"
                )
            }
        );
    } else {
        pianoKey = row(
            Modifier()
                .setfixedWidth(128)
                .setColor(isSharp ? sf::Color::Black : sf::Color::White)
                .align(Align::RIGHT),
            contains{}
        );
    }
    
    pianoKeyElements[midiNoteNumber] = pianoKey;
    
    return row(
        Modifier().setfixedHeight(32).align(Align::LEFT),
        contains{
            scrollableRowElement,
            pianoKey,
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

int PianoRoll::calculateNoteNumber(const std::string& noteName, int octave) const {
    int noteOffset = 0;
    
    if (noteName == "C") noteOffset = 0;
    else if (noteName == "C#") noteOffset = 1;
    else if (noteName == "D") noteOffset = 2;
    else if (noteName == "D#") noteOffset = 3;
    else if (noteName == "E") noteOffset = 4;
    else if (noteName == "F") noteOffset = 5;
    else if (noteName == "F#") noteOffset = 6;
    else if (noteName == "G") noteOffset = 7;
    else if (noteName == "G#") noteOffset = 8;
    else if (noteName == "A") noteOffset = 9;
    else if (noteName == "A#") noteOffset = 10;
    else if (noteName == "B") noteOffset = 11;
    
    return (octave + 1) * 12 + noteOffset;
}

void PianoRoll::updateSelectedMIDIClip() {
    MIDIClip* prevSelectedClip = selectedMIDIClip;
    selectedMIDIClip = nullptr;
    clipDuration = 1.0f;
    
    if (app) {
        selectedMIDIClip = app->getTimelineSelectedMIDIClip();
        
        if (selectedMIDIClip != prevSelectedClip) {
            if (selectedMIDIClip) {
                setInitialZoomForClip();
            } else {
                selectedMIDIClip = app->getSelectedMIDIClip();
                if (selectedMIDIClip) {
                    setInitialZoomForClip();
                }
            }
        }
        
        if (selectedMIDIClip) {
            clipDuration = selectedMIDIClip->duration;
        }
    }
}

void PianoRoll::updateKeyboardHighlighting() {
    if (!pianoRollShown) return;
    
    // Reset all piano keys to their default colors first
    for (const auto& [noteNumber, pianoKeyElement] : pianoKeyElements) {
        if (pianoKeyElement) {
            std::string noteName = notes[noteNumber % 12];
            bool isSharp = noteName.find("#") != std::string::npos;
            sf::Color defaultColor = isSharp ? sf::Color::Black : sf::Color::White;
            pianoKeyElement->m_modifier.setColor(defaultColor);
        }
    }
    
    // Apply highlight colors to currently pressed keys
    for (int noteNumber : currentlyPressedNotes) {
        auto it = pianoKeyElements.find(noteNumber);
        if (it != pianoKeyElements.end() && it->second) {
            sf::Color highlightColor = app->resources.activeTheme->clip_color;
            it->second->m_modifier.setColor(highlightColor);
        }
    }
}

void PianoRoll::scrollToNote(int noteNumber) {
    if (!baseColumn) return;
    
    int rowIndex = 127 - noteNumber;
    float noteY = rowIndex * 32.0f;
    
    sf::Vector2f columnSize = baseColumn->getSize();
    float viewportHeight = columnSize.y;
    
    if (viewportHeight <= 0) {
        viewportHeight = 600.0f;
    }
    
    float viewportCenter = viewportHeight / 2.0f;
    float targetOffset = -(noteY - viewportCenter);
    
    float maxUpwardScroll = 0.0f;
    float maxDownwardScroll = -(127 * 32.0f - viewportHeight);
    
    targetOffset = std::min(maxUpwardScroll, std::max(maxDownwardScroll, targetOffset));
    
    baseColumn->setOffset(targetOffset);
    columnOffset = targetOffset;
}

void PianoRoll::handleNoteClick(int noteNumber, float xPosition, bool isRightClick) {
    if (!selectedMIDIClip) return;
    
    float rawClipTime = xPositionToClipTime(xPosition);
    bool shiftPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
    float clipTime = shiftPressed ? rawClipTime : snapToGrid(rawClipTime);
    
    if (clipTime < 0.0f || clipTime >= clipDuration) return;
    
    float timelineTime = clipTime;
    
    if (isRightClick) {
        deleteNoteAtTime(noteNumber, rawClipTime);
    } else {
        float noteDuration = getSubmeasureDuration();
        selectedMIDIClip->addNote(noteNumber, 1.0f, timelineTime, noteDuration);
    }
}

float PianoRoll::xPositionToClipTime(float xPosition) const {
    if (clipDuration <= 0.001f) return 0.0f;
    
    const float pixelsPerSecond = getCurrentPixelsPerSecond();
    float clipRelativeTime = (xPosition - pianoRollOffset) / pixelsPerSecond;
    
    // Don't clamp to clipDuration when dragging to allow extending beyond clip length
    if (isDraggingNote) {
        return std::max(0.0f, clipRelativeTime);
    } else {
        return std::max(0.0f, std::min(clipRelativeTime, clipDuration));
    }
}

float PianoRoll::snapToGrid(float time) const {
    if (clipDuration <= 0.001f) return 0.0f;
    
    // Get submeasure duration (1/4 of a beat)
    float submeasureDuration = getSubmeasureDuration();
    
    // Always round DOWN to the start of the current submeasure
    float snappedTime = std::floor(time / submeasureDuration) * submeasureDuration;
    
    // Ensure we stay within bounds
    return std::max(0.0f, std::min(snappedTime, clipDuration));
}

float PianoRoll::getSubmeasureDuration() const {
    const double bpm = app->getBpm();
    const double beatDuration = 60.0 / bpm; // Duration of one beat in seconds
    return static_cast<float>(beatDuration / 4.0); // 1/4 of a beat (submeasure duration)
}

void PianoRoll::deleteNoteAtTime(int noteNumber, float timelineTime) {
    if (!selectedMIDIClip) return;
    
    juce::MidiBuffer newMidiData;
    int sampleRate = 44100;
    int targetSample = static_cast<int>(timelineTime * sampleRate);
    float submeasureDuration = getSubmeasureDuration();
    
    int noteOnSampleToDelete = -1;
    int noteOffSampleToDelete = -1;
    
    for (const auto& event : selectedMIDIClip->midiData) {
        const juce::MidiMessage& message = event.getMessage();
        if (message.isNoteOn() && message.getNoteNumber() == noteNumber) {
            int noteOnSample = event.samplePosition;
            int noteOffSample = noteOnSample + static_cast<int>(submeasureDuration * sampleRate);
            
            for (const auto& offEvent : selectedMIDIClip->midiData) {
                const juce::MidiMessage& offMessage = offEvent.getMessage();
                if (offMessage.isNoteOff() && 
                    offMessage.getNoteNumber() == noteNumber &&
                    offEvent.samplePosition > noteOnSample) {
                    noteOffSample = offEvent.samplePosition;
                    break;
                }
            }
            
            const int tolerance = static_cast<int>(0.001f * sampleRate);
            if (targetSample >= (noteOnSample - tolerance) && targetSample <= (noteOffSample + tolerance)) {
                noteOnSampleToDelete = noteOnSample;
                noteOffSampleToDelete = noteOffSample;
                break;
            }
        }
    }
    
    if (noteOnSampleToDelete == -1) return;
    
    for (const auto& event : selectedMIDIClip->midiData) {
        const juce::MidiMessage& message = event.getMessage();
        
        if ((message.isNoteOn() && message.getNoteNumber() == noteNumber && event.samplePosition == noteOnSampleToDelete) ||
            (message.isNoteOff() && message.getNoteNumber() == noteNumber && event.samplePosition == noteOffSampleToDelete)) {
            continue;
        }
        
        newMidiData.addEvent(message, event.samplePosition);
    }
    
    selectedMIDIClip->midiData = std::move(newMidiData);
}

int PianoRoll::getNoteNumberFromRowName(const std::string& noteName) const {
    size_t octavePos = noteName.find_first_of("0123456789");
    if (octavePos == std::string::npos) return 60;
    
    std::string baseNote = noteName.substr(0, octavePos);
    int octave = std::stoi(noteName.substr(octavePos));
    
    return calculateNoteNumber(baseNote, octave);
}

std::vector<std::shared_ptr<sf::Drawable>> PianoRoll::generateNoteRectsForRow(int targetNoteNumber, const sf::Vector2f& rowSize) const {
    std::vector<std::shared_ptr<sf::Drawable>> noteRects;
    
    if (!selectedMIDIClip || clipDuration <= 0.001f) return noteRects;
    
    const float pixelsPerSecond = getCurrentPixelsPerSecond();
        
    std::vector<std::pair<float, float>> noteSpans;
    
    for (const auto& event : selectedMIDIClip->midiData) {
        const juce::MidiMessage& message = event.getMessage();
        
        if (message.isNoteOn() && message.getNoteNumber() == targetNoteNumber) {
            // Note time is relative to the clip (0 to clipDuration)
            float noteStartTime = static_cast<float>(event.samplePosition) / 44100.0f;
            if (noteStartTime < 0.0f || noteStartTime >= clipDuration) continue;
            
            float noteEndTime = noteStartTime + 0.25f;
            
            for (const auto& offEvent : selectedMIDIClip->midiData) {
                const juce::MidiMessage& offMessage = offEvent.getMessage();
                if (offMessage.isNoteOff() && 
                    offMessage.getNoteNumber() == targetNoteNumber &&
                    offEvent.samplePosition > event.samplePosition) {
                    noteEndTime = static_cast<float>(offEvent.samplePosition) / 44100.0f;
                    break;
                }
            }
            
            noteSpans.push_back({noteStartTime, noteEndTime});
        }
    }
    
    for (const auto& span : noteSpans) {
        float startTime = span.first;
        float endTime = span.second;
        float duration = endTime - startTime;
        
        // Position notes relative to the clip's timeline position
        float xPosition = (startTime * pixelsPerSecond) + pianoRollOffset;
        float width = std::max(20.0f, duration * pixelsPerSecond);
        
        auto noteRect = std::make_shared<sf::RectangleShape>();
        noteRect->setPosition(sf::Vector2f(xPosition, 1.0f));
        noteRect->setSize(sf::Vector2f(width, 30.0f));
        noteRect->setFillColor(app->resources.activeTheme->clip_color);
        noteRect->setOutlineThickness(1.0f);
        
        sf::Color outlineColor = app->resources.activeTheme->clip_color;
        outlineColor.r = static_cast<uint8_t>(outlineColor.r * 0.7f);
        outlineColor.g = static_cast<uint8_t>(outlineColor.g * 0.7f);
        outlineColor.b = static_cast<uint8_t>(outlineColor.b * 0.7f);
        noteRect->setOutlineColor(outlineColor);
        
        noteRects.push_back(noteRect);
    }
    
    return noteRects;
}

void PianoRoll::handleNoteDragStart(int noteNumber, float xPosition) {
    if (!selectedMIDIClip) return;
    
    float rawClipTime = xPositionToClipTime(xPosition);
    bool shiftPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
    float clipTime = shiftPressed ? rawClipTime : snapToGrid(rawClipTime);
    
    if (clipTime < 0.0f || clipTime >= clipDuration) return;
    
    float noteDuration = getSubmeasureDuration();
    selectedMIDIClip->addNote(noteNumber, 1.0f, clipTime, noteDuration);
    
    isDraggingNote = true;
    draggingNoteNumber = noteNumber;
    dragStartTime = clipTime;
}

void PianoRoll::handleNoteDragUpdate() {
    if (!isDraggingNote || !selectedMIDIClip || !app->ui) {
        return;
    }
    
    if (!sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
        handleNoteDragEnd();
        return;
    }
    
    sf::Vector2f currentMousePos = app->ui->getMousePosition();
    std::string noteRowName = notes[draggingNoteNumber % 12] + std::to_string(draggingNoteNumber / 12 - 1);
    auto noteRowIter = noteRows.find(noteRowName);
    if (noteRowIter == noteRows.end() || !noteRowIter->second) return;
    
    sf::Vector2f rowPos = noteRowIter->second->getPosition();
    float relativeX = currentMousePos.x - rowPos.x;
    float currentClipTime = xPositionToClipTime(relativeX);
    float submeasureDuration = getSubmeasureDuration();
    float snappedEndTime = std::ceil(currentClipTime / submeasureDuration) * submeasureDuration;
    float minEndTime = dragStartTime + submeasureDuration;
    snappedEndTime = std::max(snappedEndTime, minEndTime);
    snappedEndTime = std::min(snappedEndTime, clipDuration);
    
    juce::MidiBuffer newMidiData;
    int dragStartSample = static_cast<int>(dragStartTime * 44100.0f);
    int newEndSample = static_cast<int>(snappedEndTime * 44100.0f);
    
    for (const auto& event : selectedMIDIClip->midiData) {
        const juce::MidiMessage& message = event.getMessage();
        
        if (message.isNoteOff() && 
            message.getNoteNumber() == draggingNoteNumber &&
            event.samplePosition > dragStartSample) {
            
            bool foundMatchingNoteOn = false;
            for (const auto& checkEvent : selectedMIDIClip->midiData) {
                const juce::MidiMessage& checkMessage = checkEvent.getMessage();
                if (checkMessage.isNoteOn() && 
                    checkMessage.getNoteNumber() == draggingNoteNumber &&
                    std::abs(checkEvent.samplePosition - dragStartSample) < 1000) {
                    foundMatchingNoteOn = true;
                    break;
                }
            }
            
            if (foundMatchingNoteOn) {
                newMidiData.addEvent(message, newEndSample);
            } else {
                newMidiData.addEvent(message, event.samplePosition);
            }
        } else {
            newMidiData.addEvent(message, event.samplePosition);
        }
    }

    selectedMIDIClip->midiData = std::move(newMidiData);
    lastMeasureWidth = -1.0f;
    lastScrollOffset = -1.0f;
    lastRowSize = {-1.0f, -1.0f};
}

void PianoRoll::handleNoteDragEnd() {
    isDraggingNote = false;
    draggingNoteNumber = -1;
    dragStartTime = 0.0f;
}

void PianoRoll::handleZoom(float verticalDelta) {
    if (!app || !app->ui) return;
    
    if (std::abs(verticalDelta) > 0.1f) {
        bool ctrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || 
                          sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
        
        if (ctrlPressed) {
            const float minZoom = 0.1f;
            const float maxZoom = 10.0f;
            const float currentZoom = pianoRollZoomLevel;
            
            // Use the same adaptive zoom speed as timeline
            const float baseSpeed = 0.35f;
            float speedMultiplier = 1.0f;
            
            // Normalize zoom level (0.1 to 10.0 -> 0.0 to 1.0)
            float normalizedZoom = (currentZoom - minZoom) / (maxZoom - minZoom);
            
            if (normalizedZoom > 0.88f) {
                float nearMaxFactor = (normalizedZoom - 0.88f) / 0.12f;
                speedMultiplier = 1.0f - (nearMaxFactor * 0.5f);
            }
            
            float adaptiveZoomSpeed = baseSpeed * speedMultiplier;
            adaptiveZoomSpeed = std::max(0.015f, adaptiveZoomSpeed);
            
            float newZoom = currentZoom + (verticalDelta > 0 ? adaptiveZoomSpeed : -adaptiveZoomSpeed);
            newZoom = std::clamp(newZoom, minZoom, maxZoom);
            
            if (newZoom != pianoRollZoomLevel) {
                // Calculate mouse position for zoom centering
                sf::Vector2f mousePos = app->ui->getMousePosition();
                sf::Vector2f basePos = baseColumn->getPosition();
                sf::Vector2f localMousePos = mousePos - basePos;
                
                // Calculate what time is currently under the mouse cursor
                const float oldPixelsPerSecond = getCurrentPixelsPerSecond();
                const float timeAtMouse = (localMousePos.x - pianoRollOffset) / oldPixelsPerSecond;
                
                // Update zoom level (piano roll's own zoom)
                pianoRollZoomLevel = newZoom;
                
                // Calculate new offset so that same time is under mouse cursor
                const float newPixelsPerSecond = getCurrentPixelsPerSecond();
                pianoRollOffset = localMousePos.x - (timeAtMouse * newPixelsPerSecond);
                
                // Apply new offset to all rows
                for (const auto& [noteName, scrollableRow] : noteRows) {
                    if (scrollableRow) {
                        scrollableRow->setOffset(pianoRollOffset);
                    }
                }
                
                // Force cache invalidation
                lastMeasureWidth = -1.0f;
                lastScrollOffset = -1.0f;
                lastRowSize = {-1.0f, -1.0f};
                lastZoomLevel = -1.0f;
            }
            
            // Block vertical scrolling when zooming - consume the scroll event
            return;
        }
    }
}

float PianoRoll::getCurrentPixelsPerSecond() const {
    const float beatWidth = 100.f * pianoRollZoomLevel;
    const double bpm = app->getBpm();
    return (beatWidth * bpm) / 60.0f;
}

void PianoRoll::setInitialZoomForClip() {
    if (!selectedMIDIClip || !baseColumn || clipDuration <= 0.001f) return;
    
    // Get the available width for piano notes (excluding the piano key labels)
    const sf::Vector2f baseSize = baseColumn->getSize();
    float availableWidth = baseSize.x;
    
    // Account for piano key label width (approximately 128 pixels)
    float pianoNotesWidth = availableWidth - 128.0f;
    
    if (pianoNotesWidth <= 0) return;
    
    // Calculate zoom level needed to fit the clip end at the edge of piano notes
    const double bpm = app->getBpm();
    const float targetPixelsPerSecond = pianoNotesWidth / clipDuration;
    
    // Calculate required zoom level: pixelsPerSecond = (beatWidth * bpm) / 60.0f
    // beatWidth = 100.f * zoomLevel
    // So: targetPixelsPerSecond = (100.f * zoomLevel * bpm) / 60.0f
    // Therefore: zoomLevel = (targetPixelsPerSecond * 60.0f) / (100.f * bpm)
    const float targetZoom = (targetPixelsPerSecond * 60.0f) / (100.f * bpm);
    
    // Clamp to reasonable zoom range
    const float clampedZoom = std::clamp(targetZoom, 0.1f, 10.0f);
    
    // Set the zoom level
    pianoRollZoomLevel = clampedZoom;
    
    // Reset piano roll offset to start at beginning of clip
    pianoRollOffset = 0.0f;
    
    // Apply offset to all rows
    for (const auto& [noteName, scrollableRow] : noteRows) {
        if (scrollableRow) {
            scrollableRow->setOffset(pianoRollOffset);
        }
    }
    
    // Force cache invalidation
    lastMeasureWidth = -1.0f;
    lastScrollOffset = -1.0f;
    lastRowSize = {-1.0f, -1.0f};
    lastZoomLevel = -1.0f;
}

GET_INTERFACE
DECLARE_PLUGIN(PianoRoll)
