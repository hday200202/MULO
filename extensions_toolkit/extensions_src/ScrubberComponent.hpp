#pragma once
#include "MULOComponent.hpp"
#include "Application.hpp"
#include "../../src/DebugConfig.hpp"

class ScrubberComp : public MULOComponent {
public:
    inline ScrubberComp(){ name = "Scrubber";}
    ~ScrubberComp() override{}

    void init() override;
    bool handleEvents() override;
    void update() override;
    float grabValue();
    bool isDraggingScrubber() const { return isDragging; }

private:
    Row* scrubberRow = nullptr;
    float lastValue = 0.f;
    bool isDragging = false;
    sf::Vector2f dragStartMousePos;
    float dragStartValue = 0.f;
    float dragOffsetInRect = 0.f; // Offset of mouse click within the rectangle
    std::vector<std::shared_ptr<sf::Drawable>> scrubberGeometry;
};

void ScrubberComp::init() {
    return;
    MULOComponent* timeline = app->getComponent("timeline");

    if (!timeline) return;

    // Create a simple row container for our custom scrubber
    scrubberRow = row(
        Modifier()
            .setWidth(1.f)
            .setfixedHeight(32)
            .align(Align::CENTER_X | Align::CENTER_Y)
            .setColor(app->resources.activeTheme->track_color),
        contains{}, "scrubber_row"
    );

    layout = row(
        Modifier()
            .align(Align::LEFT | Align::TOP)
            .setfixedHeight(48.f)
            .setColor(app->resources.activeTheme->track_color),
    contains{
        scrubberRow,  
    });

    parentContainer = timeline->getLayout();

    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

bool ScrubberComp::handleEvents() {
    return false;
}

void ScrubberComp::update() {
    if (!scrubberRow) return;
    
    float widthRatio = app->readConfig<float>("scrubber_width_ratio", 0.0f);
    float scrubberPosition = app->readConfig<float>("scrubber_position", 0.0f);
    
    sf::Vector2f mousePos = app->ui->getMousePosition();
    sf::Vector2f rowPos = scrubberRow->getPosition();
    sf::Vector2f rowSize = scrubberRow->getSize();
    sf::FloatRect rowBounds(rowPos, rowSize);
    
    bool mouseOverRow = rowBounds.contains(mousePos);
    bool mousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
    bool mouseDragging = app->ui->isMouseDragging();
    
    if (mouseOverRow && mousePressed && !isDragging) {
        isDragging = true;
        dragStartMousePos = mousePos;
        dragStartValue = scrubberPosition;
        
        float widthRatio = app->readConfig<float>("scrubber_width_ratio", 0.0f);
        if (widthRatio > 0.0f) {
            float totalWidth = rowSize.x;
            float rectWidth = widthRatio * totalWidth;
            float rectX = scrubberPosition * totalWidth - (scrubberPosition * rectWidth);
            rectX = std::max(0.0f, std::min(rectX, totalWidth - rectWidth));
            
            float mouseXInRect = mousePos.x - (rowPos.x + rectX);
            dragOffsetInRect = mouseXInRect / rectWidth;
            dragOffsetInRect = std::max(0.0f, std::min(1.0f, dragOffsetInRect));
        } else {
            dragOffsetInRect = 0.5f;
        }
        
        app->writeConfig<bool>("scrubber_dragging", true);
    }
    
    if (isDragging && mouseDragging) {
        float widthRatio = app->readConfig<float>("scrubber_width_ratio", 0.0f);
        if (widthRatio > 0.0f) {
            float totalWidth = rowSize.x;
            float rectWidth = widthRatio * totalWidth;
            float mouseOffsetInRectPixels = dragOffsetInRect * rectWidth;
            float targetRectX = (mousePos.x - rowPos.x) - mouseOffsetInRectPixels;
            targetRectX = std::max(0.0f, std::min(targetRectX, totalWidth - rectWidth));
            
            float newPosition;
            if (rectWidth < totalWidth) {
                newPosition = targetRectX / (totalWidth - rectWidth);
            } else {
                newPosition = 0.0f;
            }
            
            newPosition = std::max(0.0f, std::min(1.0f, newPosition));
            app->writeConfig<float>("scrubber_position", newPosition);
            lastValue = newPosition;
        }
    }
    
    if (isDragging && !mousePressed) {
        isDragging = false;
        app->writeConfig<bool>("scrubber_dragging", false);
    }
    
    if (!isDragging) {
        float configValue = app->readConfig<float>("scrubber_position", 0.0f);
        if (std::abs(configValue - lastValue) > 0.001f) {
            lastValue = configValue;
        }
        scrubberPosition = lastValue;
    }

    scrubberGeometry.clear();
    
    if (widthRatio > 0.0f && widthRatio <= 1.0f && rowSize.x > 0) {
        float totalWidth = rowSize.x;
        float rectWidth = widthRatio * totalWidth;
        float rectX = scrubberPosition * totalWidth - (scrubberPosition * rectWidth);
        rectX = std::max(0.0f, std::min(rectX, totalWidth - rectWidth));
        
        // Check if mouse is over the rectangle
        sf::FloatRect rectBounds({rowPos.x + rectX, rowPos.y}, {rectWidth, rowSize.y});
        bool mouseOverRect = rectBounds.contains(mousePos);
        bool isHighlighted = mouseOverRect || isDragging;
        
        auto viewportRect = std::make_shared<sf::RectangleShape>();
        viewportRect->setSize({rectWidth, rowSize.y * 0.8f});
        viewportRect->setPosition({rectX, rowSize.y * 0.1f});
        
        sf::Color rectColor = app->resources.activeTheme->foreground_color;
        if (isHighlighted) {
            // Brighten the color
            rectColor.r = std::min(255, (int)(rectColor.r * 1.3f));
            rectColor.g = std::min(255, (int)(rectColor.g * 1.3f));
            rectColor.b = std::min(255, (int)(rectColor.b * 1.3f));
        }
        viewportRect->setFillColor(rectColor);
        
        scrubberGeometry.push_back(viewportRect);
    }
    
    scrubberRow->setCustomGeometry(scrubberGeometry);
}

float ScrubberComp::grabValue() {
    return lastValue;
}

GET_INTERFACE
DECLARE_PLUGIN(ScrubberComp);