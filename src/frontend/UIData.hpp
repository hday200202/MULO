#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <SFML/Graphics/Color.hpp>
#include "Engine.hpp"

struct TrackData {
    std::string name;
    float volume = 1.0f;
    float pan = 0.0f;
    std::vector<AudioClip> clips;

    TrackData(const std::string& trackName = "Master")
        : name(trackName) {}
};

struct UIState {
    std::string fileBrowserDirectory = "";
    std::string selectedTrackName   = "Master";
    int         trackCount          = 0;
    float timelineZoomLevel = 1.f;
    int         autoSaveIntervalSeconds = 300; // default 5 minutes

    TrackData masterTrack{"Master"};
    std::unordered_map<std::string, TrackData> tracks;
};

struct UIResources {
    std::string dejavuSansFont;
    std::string spaceMonoFont;
    std::string ubuntuBoldFont;
    std::string ubuntuMonoFont;
    std::string ubuntuMonoBoldFont;
    // Add more resources as needed
};

// Colors
static const sf::Color button_color = sf::Color::Red;
static const sf::Color track_color = sf::Color(155, 155, 155);
static const sf::Color track_row_color = sf::Color(120, 120, 120);
static const sf::Color master_track_color = sf::Color(155, 155, 155);
static const sf::Color mute_color = sf::Color::Red;
static const sf::Color foreground_color = sf::Color(200, 200, 200);
static const sf::Color primary_text_color = sf::Color::Black;
static const sf::Color secondary_text_color = sf::Color::White;
static const sf::Color not_muted_color = sf::Color(50, 50, 50);
static const sf::Color middle_color = sf::Color(100, 100, 100);
static const sf::Color alt_button_color = sf::Color(120, 120, 120);
static const sf::Color white = sf::Color::White;
static const sf::Color black = sf::Color::Black;
static const sf::Color slider_knob_color = sf::Color::White;
static const sf::Color slider_bar_color = sf::Color::Black;

// Theme Structure
struct UITheme {
    sf::Color buttonColor;
    sf::Color trackColor;
    sf::Color trackRowColor;
    sf::Color masterTrackColor;
    sf::Color muteColor;
    sf::Color foregroundColor;
    sf::Color primaryTextColor;
    sf::Color secondaryTextColor;
    sf::Color notMutedColor;
    sf::Color middleColor;
    sf::Color altButtonColor;
    sf::Color white;
    sf::Color black;
    sf::Color sliderKnobColor;
    sf::Color sliderBarColor;
    sf::Color clipColor;
    sf::Color lineColor;
    sf::Color waveformColor;
    
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
        sf::Color waveform = sf::Color(0, 150, 255)
    ) : buttonColor(btn), trackColor(track), trackRowColor(trackRow), masterTrackColor(masterTrack),
        muteColor(mute), foregroundColor(fg), primaryTextColor(primaryText), secondaryTextColor(secondaryText),
        notMutedColor(notMuted), middleColor(middle), altButtonColor(altBtn), white(w), black(b),
        sliderKnobColor(sliderKnob), sliderBarColor(sliderBar), clipColor(clip), lineColor(line), waveformColor(waveform) {}
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
        sf::Color(105, 125, 150),     // sliderKnobColor - Muted Light Blue
        sf::Color(30, 30, 30),        // sliderBarColor - Dark
        sf::Color(90, 120, 160),      // clipColor - Muted Blue
        sf::Color(100, 100, 100),     // lineColor - Medium Gray
        sf::Color(170, 190, 230)      // waveformColor - Soft Blue
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
        sf::Color(80, 140, 200)       // waveformColor - Blue
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
        sf::Color(140, 100, 160)      // waveformColor - Soft Purple
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
        sf::Color(120, 150, 100)      // waveformColor - Light Green
    );
}

// Current active theme (defaults to Default)
static UITheme currentTheme = Themes::Dark;

// Dynamic color macros (use current theme)
#define button_color currentTheme.buttonColor
#define track_color currentTheme.trackColor
#define track_row_color currentTheme.trackRowColor
#define master_track_color currentTheme.masterTrackColor
#define mute_color currentTheme.muteColor
#define foreground_color currentTheme.foregroundColor
#define primary_text_color currentTheme.primaryTextColor
#define secondary_text_color currentTheme.secondaryTextColor
#define not_muted_color currentTheme.notMutedColor
#define middle_color currentTheme.middleColor
#define alt_button_color currentTheme.altButtonColor
#define white currentTheme.white
#define black currentTheme.black
#define slider_knob_color currentTheme.sliderKnobColor
#define slider_bar_color currentTheme.sliderBarColor
#define clip_color currentTheme.clipColor
#define line_color currentTheme.lineColor
#define waveform_color currentTheme.waveformColor

// Helper function to apply a theme
inline void applyTheme(const UITheme& theme) {
    currentTheme = theme;
}