
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <iostream>
#include <filesystem>
#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
#include "Engine.hpp"
#include "../DebugConfig.hpp"
#include "UILO/UILO.hpp"

using json = nlohmann::json;

struct UITheme;

struct UIState {
    std::string fileBrowserDirectory = "";
    std::string vstDirecory = "";
    std::vector<std::string> vstDirectories = {};
    std::string saveDirectory = "";
    std::string selectedTheme = "Dark";

    float timelineZoomLevel = 1.f;
    double sampleRate = 44100.0;
    int autoSaveIntervalSeconds = 300;
    bool settingsShown = false;
    bool marketplaceShown = false;
    bool enableAutoVSTScan = false;

    inline void printUIState() {
        DEBUG_PRINT("  [File Browser Dir] " << fileBrowserDirectory);
        DEBUG_PRINT("     [VST Directory] " << vstDirecory);
        DEBUG_PRINT("          [Save Dir] " << saveDirectory);
        DEBUG_PRINT("          [UI Theme] " << selectedTheme);
        DEBUG_PRINT("       [Sample Rate] " << sampleRate);
        DEBUG_PRINT("[Auto Save Interval] " << autoSaveIntervalSeconds);
    }

    inline std::string getExecutableDirectory() {
        try {
            std::filesystem::path exePath = std::filesystem::current_path();
            return exePath.string();
        } catch (const std::exception& e) {
            DEBUG_PRINT("Error getting executable directory: " << e.what());
            return ".";
        }
    }
};

struct UIResources {
    std::string dejavuSansFont;
    std::string spaceMonoFont;
    std::string ubuntuBoldFont;
    std::string ubuntuMonoFont;
    std::string ubuntuMonoBoldFont;

    sf::Image playIcon;
    sf::Image pauseIcon;
    sf::Image settingsIcon;
    sf::Image pianoRollIcon;
    sf::Image loadIcon;
    sf::Image saveIcon;
    sf::Image exportIcon;
    sf::Image folderIcon;
    sf::Image openFolderIcon;
    sf::Image pluginFileIcon;
    sf::Image audioFileIcon;
    sf::Image metronomeIcon;
    sf::Image mixerIcon;
    sf::Image storeIcon;
    sf::Image fileIcon;

    UITheme* activeTheme = nullptr;
    // Add more resources as needed
};

struct ComponentLayoutData {
    uilo::Container* parent = nullptr;
    uilo::Align alignment = uilo::Align::LEFT;
    std::string relativeTo = "";
};

struct UITheme {
    sf::Color button_color;
    sf::Color track_color;
    sf::Color track_row_color;
    sf::Color master_track_color;
    sf::Color mute_color;
    sf::Color foreground_color;
    sf::Color primary_text_color;
    sf::Color secondary_text_color;
    sf::Color not_muted_color;
    sf::Color middle_color;
    sf::Color alt_button_color;
    sf::Color white;
    sf::Color black;
    sf::Color slider_knob_color;
    sf::Color slider_bar_color;
    sf::Color clip_color;
    sf::Color line_color;
    sf::Color wave_form_color;
    sf::Color selected_track_color;
    
    UITheme(
        sf::Color btn = sf::Color::Red,
        sf::Color track = sf::Color(155, 155, 155),
        sf::Color trackRow = sf::Color(120, 120, 120),
        sf::Color masterTrack = sf::Color(155, 155, 155),
        sf::Color mute = sf::Color::Red,
        sf::Color fg = sf::Color(200, 200, 200),
        sf::Color primaryText = sf::Color::Black,
        sf::Color secondaryText = sf::Color::White,
        sf::Color notMuted = sf::Color(50, 50, 50),
        sf::Color middle = sf::Color(100, 100, 100),
        sf::Color altBtn = sf::Color(120, 120, 120),
        sf::Color w = sf::Color::White,
        sf::Color b = sf::Color::Black,
        sf::Color sliderKnob = sf::Color::White,
        sf::Color sliderBar = sf::Color::Black,
        sf::Color clip = sf::Color(100, 150, 200),
        sf::Color line = sf::Color(80, 80, 80),
        sf::Color waveform = sf::Color(0, 150, 255),
        sf::Color selectedTrack = sf::Color(100, 150, 200)
    ) : button_color(btn), track_color(track), track_row_color(trackRow), master_track_color(masterTrack),
        mute_color(mute), foreground_color(fg), primary_text_color(primaryText), secondary_text_color(secondaryText),
        not_muted_color(notMuted), middle_color(middle), alt_button_color(altBtn), white(w), black(b),
        slider_knob_color(sliderKnob), slider_bar_color(sliderBar), clip_color(clip), line_color(line), 
        wave_form_color(waveform), selected_track_color(selectedTrack) {}
};

namespace Themes {
    const UITheme Default = UITheme(
        sf::Color::Red,               // buttonColor
        sf::Color(155, 155, 155),     // trackColor  
        sf::Color(120, 120, 120),     // trackRowColor
        sf::Color(155, 155, 155),     // masterTrackColor
        sf::Color::Red,               // muteColor
        sf::Color(200, 200, 200),     // foregroundColor
        sf::Color::Black,             // primaryTextColor
        sf::Color::White,             // secondaryTextColor
        sf::Color(50, 50, 50),        // notMutedColor
        sf::Color(100, 100, 100),     // middleColor
        sf::Color(120, 120, 120),     // altButtonColor
        sf::Color::White,             // white
        sf::Color::Black,             // black
        sf::Color::White,             // sliderKnobColor
        sf::Color::Black,             // sliderBarColor
        sf::Color::White,             // clipColor - Light Blue
        sf::Color(80, 80, 80),        // lineColor - Gray
        sf::Color::Black              // waveformColor - Blue
    );
    
    // Dark Theme
    const UITheme Dark = UITheme(
        sf::Color(85, 115, 140),      // buttonColor - Muted Blue
        sf::Color(60, 60, 60),        // trackColor - Dark Gray
        sf::Color(45, 45, 45),        // trackRowColor - Darker Gray
        sf::Color(80, 80, 80),        // masterTrackColor - Medium Gray
        sf::Color(140, 70, 70),       // muteColor - Muted Red
        sf::Color(70, 70, 70),        // foregroundColor - Dark Gray
        sf::Color(230, 230, 230),     // primaryTextColor - Soft White
        sf::Color(230, 230, 230),     // secondaryTextColor - Muted Light Gray
        sf::Color(30, 30, 30),        // notMutedColor
        sf::Color(40, 40, 40),        // middleColor
        sf::Color(50, 50, 50),        // altButtonColor
        sf::Color::White,             // white
        sf::Color(20, 20, 20),        // black - Very Dark
        sf::Color::White,             // sliderKnobColor - Muted Light Blue
        sf::Color(30, 30, 30),        // sliderBarColor - Dark
        sf::Color(90, 120, 160),      // clipColor - Muted Blue
        sf::Color(100, 100, 100),     // lineColor - Medium Gray
        sf::Color::White,             // waveformColor - Soft Blue
        sf::Color(90, 120, 160)       // selectedTrackColor - Muted Blue
    );
    
    // Light Theme
    const UITheme Light = UITheme(
        sf::Color(90, 130, 160),      // buttonColor - Muted Blue
        sf::Color(245, 245, 245),     // trackColor - Very Light Gray
        sf::Color(235, 235, 235),     // trackRowColor - Light Gray
        sf::Color(220, 220, 220),     // masterTrackColor - Medium Light Gray
        sf::Color(160, 80, 80),       // muteColor - Muted Red
        sf::Color(250, 250, 250),     // foregroundColor - Off White
        sf::Color(40, 40, 40),        // primaryTextColor - Dark Gray
        sf::Color(80, 80, 80),        // secondaryTextColor - Medium Gray
        sf::Color(180, 180, 180),     // notMutedColor
        sf::Color(200, 200, 200),     // middleColor
        sf::Color(160, 160, 160),     // altButtonColor
        sf::Color::White,             // white
        sf::Color::Black,             // black
        sf::Color(70, 110, 140),      // sliderKnobColor - Muted Dark Blue
        sf::Color(215, 215, 215),     // sliderBarColor - Soft Gray
        sf::Color(120, 160, 200),     // clipColor - Light Blue
        sf::Color(120, 120, 120),     // lineColor - Medium Gray
        sf::Color(80, 140, 200),      // waveformColor - Blue
        sf::Color(120, 160, 200)      // selectedTrackColor - Light Blue
    );
    
    // Cyberpunk Theme
    const UITheme Cyberpunk = UITheme(
        sf::Color(160, 80, 120),      // buttonColor - Muted Pink
        sf::Color(55, 50, 65),        // trackColor - Muted Purple
        sf::Color(45, 40, 55),        // trackRowColor - Dark Muted Purple
        sf::Color(70, 60, 80),        // masterTrackColor - Soft Purple
        sf::Color(140, 70, 100),      // muteColor - Muted Pink
        sf::Color(40, 35, 50),        // foregroundColor - Dark Muted Blue
        sf::Color(120, 160, 160),     // primaryTextColor - Muted Cyan
        sf::Color(200, 200, 200),     // secondaryTextColor - Soft White
        sf::Color(25, 20, 35),        // notMutedColor
        sf::Color(65, 50, 80),        // middleColor - Muted Purple
        sf::Color(90, 70, 110),       // altButtonColor - Soft Purple
        sf::Color(255, 255, 255),     // white
        sf::Color(15, 10, 25),        // black - Almost Black
        sf::Color(100, 140, 130),     // sliderKnobColor - Muted Teal
        sf::Color(80, 60, 100),       // sliderBarColor - Muted Purple
        sf::Color(120, 80, 140),      // clipColor - Muted Purple-Pink
        sf::Color(80, 120, 120),      // lineColor - Muted Teal
        sf::Color(140, 100, 160),     // waveformColor - Soft Purple
        sf::Color(120, 80, 140)
    );
    
    // Forest Theme
    const UITheme Forest = UITheme(
        sf::Color(80, 110, 80),       // buttonColor - Muted Forest Green
        sf::Color(90, 100, 75),       // trackColor - Muted Olive
        sf::Color(100, 115, 85),      // trackRowColor - Soft Olive
        sf::Color(85, 85, 100),       // masterTrackColor - Muted Slate
        sf::Color(130, 70, 70),       // muteColor - Muted Red
        sf::Color(115, 125, 115),     // foregroundColor - Soft Green Gray
        sf::Color(210, 205, 190),     // primaryTextColor - Soft Beige
        sf::Color(220, 215, 200),     // secondaryTextColor - Light Beige
        sf::Color(65, 75, 75),        // notMutedColor - Muted Slate
        sf::Color(95, 95, 95),        // middleColor - Medium Gray
        sf::Color(105, 115, 125),     // altButtonColor - Soft Blue Gray
        sf::Color::White,             // white
        sf::Color(35, 35, 50),        // black - Dark Blue Gray
        sf::Color(90, 130, 90),       // sliderKnobColor - Muted Green
        sf::Color(75, 100, 75),       // sliderBarColor - Darker Muted Green
        sf::Color(100, 130, 90),      // clipColor - Sage Green
        sf::Color(110, 120, 100),     // lineColor - Olive Gray
        sf::Color(120, 150, 100),     // waveformColor - Light Green
        sf::Color(100, 130, 90)
    );
    
    // Ocean Theme
    const UITheme Ocean = UITheme(
        sf::Color(50, 120, 180),      // buttonColor - Deep Blue
        sf::Color(30, 60, 90),        // trackColor - Navy
        sf::Color(40, 80, 120),       // trackRowColor - Blue Gray
        sf::Color(60, 130, 180),      // masterTrackColor - Light Blue
        sf::Color(200, 80, 80),       // muteColor - Coral Red
        sf::Color(20, 40, 60),        // foregroundColor - Deep Navy
        sf::Color(180, 220, 240),     // primaryTextColor - Pale Blue
        sf::Color(120, 180, 200),     // secondaryTextColor - Muted Cyan
        sf::Color(30, 50, 70),        // notMutedColor
        sf::Color(40, 90, 120),       // middleColor - Blue Gray
        sf::Color(80, 180, 200),      // altButtonColor - Aqua
        sf::Color::White,             // white
        sf::Color(10, 20, 30),        // black - Deepest Blue
        sf::Color(60, 180, 200),      // sliderKnobColor - Aqua
        sf::Color(30, 90, 120),       // sliderBarColor - Blue Gray
        sf::Color(80, 180, 220),      // clipColor - Light Aqua
        sf::Color(100, 160, 200),     // lineColor - Soft Blue
        sf::Color(120, 200, 240)      // waveformColor - Bright Blue
    );
    
    // Sunset Theme
    const UITheme Sunset = UITheme(
        sf::Color(255, 120, 60),      // buttonColor - Orange
        sf::Color(200, 90, 60),       // trackColor - Burnt Orange
        sf::Color(255, 180, 120),     // trackRowColor - Peach
        sf::Color(255, 140, 80),      // masterTrackColor - Light Orange
        sf::Color(200, 60, 60),       // muteColor - Red
        sf::Color(120, 60, 40),       // foregroundColor - Brown
        sf::Color(255, 240, 220),     // primaryTextColor - Cream
        sf::Color(255, 200, 160),     // secondaryTextColor - Light Peach
        sf::Color(120, 60, 40),       // notMutedColor
        sf::Color(255, 170, 100),     // middleColor - Light Orange
        sf::Color(255, 200, 120),     // altButtonColor - Yellow Peach
        sf::Color::White,             // white
        sf::Color(60, 30, 20),        // black - Deep Brown
        sf::Color(255, 180, 80),      // sliderKnobColor - Gold
        sf::Color(200, 120, 60),      // sliderBarColor - Orange Brown
        sf::Color(255, 160, 80),      // clipColor - Orange
        sf::Color(255, 200, 120),     // lineColor - Light Yellow
        sf::Color(255, 180, 120)      // waveformColor - Peach
    );
    
    // Monochrome Theme
    const UITheme Monochrome = UITheme(
        sf::Color(120, 120, 120),     // buttonColor - Gray
        sf::Color(80, 80, 80),        // trackColor - Dark Gray
        sf::Color(100, 100, 100),     // trackRowColor - Medium Gray
        sf::Color(150, 150, 150),     // masterTrackColor - Light Gray
        sf::Color(200, 80, 80),       // muteColor - Red
        sf::Color(60, 60, 60),        // foregroundColor - Dark Gray
        sf::Color(230, 230, 230),     // primaryTextColor - White
        sf::Color(230, 230, 230),     // secondaryTextColor - Light Gray
        sf::Color(40, 40, 40),        // notMutedColor
        sf::Color(120, 120, 120),     // middleColor - Gray
        sf::Color(50, 50, 50),        // altButtonColor - Light Gray
        sf::Color::White,             // white
        sf::Color(20, 20, 20),        // black - Very Dark
        sf::Color(180, 180, 180),     // sliderKnobColor - Light Gray
        sf::Color(100, 100, 100),     // sliderBarColor - Medium Gray
        sf::Color(150, 150, 150),     // clipColor - Light Gray
        sf::Color(120, 120, 120),     // lineColor - Gray
        sf::Color(50, 50, 50)      // waveformColor - White
    );
    
    // Solarized Theme
    const UITheme Solarized = UITheme(
        sf::Color(38, 139, 210),      // buttonColor - Blue
        sf::Color(101, 123, 131),     // trackColor - Base1
        sf::Color(131, 148, 150),     // trackRowColor - Base0
        sf::Color(147, 161, 161),     // masterTrackColor - Base01
        sf::Color(220, 50, 47),       // muteColor - Red
        sf::Color(0, 43, 54),         // foregroundColor - Base02
        sf::Color(253, 246, 227),     // primaryTextColor - Base3
        sf::Color(238, 232, 213),     // secondaryTextColor - Base2
        sf::Color(88, 110, 117),      // notMutedColor - Base00
        sf::Color(133, 153, 0),       // middleColor - Green
        sf::Color(42, 161, 152),      // altButtonColor - Cyan
        sf::Color::White,             // white
        sf::Color(7, 54, 66),         // black - Base03
        sf::Color(181, 137, 0),       // sliderKnobColor - Yellow
        sf::Color(203, 75, 22),       // sliderBarColor - Orange
        sf::Color(38, 139, 210),      // clipColor - Blue
        sf::Color(42, 161, 152),      // lineColor - Cyan
        sf::Color(133, 153, 0)        // waveformColor - Green
    );

    // List of all theme names
    const std::initializer_list<std::string> AllThemeNames = {
        "Default",
        "Dark",
        "Light",
        "Cyberpunk",
        "Forest",
        "Ocean",
        "Sunset",
        "Monochrome",
        "Solarized"
    };
}

// Helper function to apply a theme
inline void applyTheme(UIResources& resources, const std::string& themeName) {
    if (themeName == "Dark") resources.activeTheme = const_cast<UITheme*>(&Themes::Dark);
    else if (themeName == "Light") resources.activeTheme = const_cast<UITheme*>(&Themes::Light);
    else if (themeName == "Cyberpunk") resources.activeTheme = const_cast<UITheme*>(&Themes::Cyberpunk);
    else if (themeName == "Forest") resources.activeTheme = const_cast<UITheme*>(&Themes::Forest);
    else if (themeName == "Ocean") resources.activeTheme = const_cast<UITheme*>(&Themes::Ocean);
    else if (themeName == "Sunset") resources.activeTheme = const_cast<UITheme*>(&Themes::Sunset);
    else if (themeName == "Monochrome") resources.activeTheme = const_cast<UITheme*>(&Themes::Monochrome);
    else if (themeName == "Solarized") resources.activeTheme = const_cast<UITheme*>(&Themes::Solarized);
    else resources.activeTheme = const_cast<UITheme*>(&Themes::Default);
}

inline std::string getAlignmentString(uilo::Align alignment) {
    int alignValue = static_cast<int>(alignment);

    constexpr int top = static_cast<int>(uilo::Align::TOP);
    constexpr int bottom = static_cast<int>(uilo::Align::BOTTOM);
    constexpr int left = static_cast<int>(uilo::Align::LEFT);
    constexpr int right = static_cast<int>(uilo::Align::RIGHT);
    constexpr int centerX = static_cast<int>(uilo::Align::CENTER_X);
    constexpr int centerY = static_cast<int>(uilo::Align::CENTER_Y);

    constexpr int topLeft = left | top;
    constexpr int topRight = right | top;
    constexpr int bottomLeft = left | bottom;
    constexpr int bottomRight = right | bottom;
    constexpr int topCenterX = top | centerX;
    constexpr int bottomCenterX = bottom | centerX;
    constexpr int leftCenterY = left | centerY;
    constexpr int rightCenterY = right | centerY;

    switch (alignValue) {
        case top:                   return "Top";
        case bottom:                return "Bottom";
        case left:                  return "Left";
        case right:                 return "Right";
        case centerX:               return "Center X";
        case centerY:               return "Center Y";
        case topLeft:               return "Top Left";
        case topRight:              return "Top Right";
        case bottomLeft:            return "Bottom Left";
        case bottomRight:           return "Bottom Right";
        case topCenterX:            return "Top Center X";
        case bottomCenterX:         return "Bottom Center X";
        case leftCenterY:           return "Left Center Y";
        case rightCenterY:          return "Right Center Y";
        default:                    return "Unknown";
    }
}