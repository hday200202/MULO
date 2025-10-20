#pragma once

#include "UILO/UILO.hpp"
#include <functional>
#include <unordered_map>

using namespace uilo;

// Forward declarations
class Application;

enum class EntryType {
    Slider,
    Dropdown,
    Toggle,
    TextBox,
    Button
};

struct Entry {
    std::string settingName = "";
    EntryType type = EntryType::Toggle;
    void* paramPtr = nullptr;
    std::function<void()> onChange = nullptr;
    
    // slider
    float minValue = 0.0f;
    float maxValue = 100.0f;
    float defaultValue = 0.0f;
    
    // dropdown/textbox
    std::string stringDefault = "";
    std::vector<std::string> options;
    int charLimit = 256;
    
    // toggle
    bool boolDefault = false;
    
    // button
    std::string buttonLabel = "";
    
    virtual ~Entry() = default;
};

// "Factory" functions
inline Entry* sliderSetting(const std::string& name, float* paramPtr, float min = 0.0f, float max = 100.0f, float defaultVal = 0.0f, std::function<void()> onChange = nullptr) {
    Entry* e = new Entry();
    e->settingName = name;
    e->type = EntryType::Slider;
    e->paramPtr = paramPtr;
    e->minValue = min;
    e->maxValue = max;
    e->defaultValue = defaultVal;
    e->onChange = onChange;
    return e;
}

inline Entry* dropdownSetting(const std::string& name, std::string* paramPtr, const std::vector<std::string>& options, const std::string& defaultVal = "", std::function<void()> onChange = nullptr) {
    Entry* e = new Entry();
    e->settingName = name;
    e->type = EntryType::Dropdown;
    e->paramPtr = paramPtr;
    e->options = options;
    e->stringDefault = defaultVal;
    e->onChange = onChange;
    return e;
}

inline Entry* toggleSetting(const std::string& name, bool* paramPtr, bool defaultVal = false, std::function<void()> onChange = nullptr) {
    Entry* e = new Entry();
    e->settingName = name;
    e->type = EntryType::Toggle;
    e->paramPtr = paramPtr;
    e->boolDefault = defaultVal;
    e->onChange = onChange;
    return e;
}

inline Entry* textboxSetting(const std::string& name, std::string* paramPtr, const std::string& initialVal = "", int charLimit = 256, std::function<void()> onChange = nullptr) {
    Entry* e = new Entry();
    e->settingName = name;
    e->type = EntryType::TextBox;
    e->paramPtr = paramPtr;
    e->stringDefault = "";
    e->charLimit = charLimit;
    e->onChange = onChange;
    // Set the initial value in the paramPtr
    if (paramPtr && !initialVal.empty()) {
        *paramPtr = initialVal;
    }
    return e;
}

inline Entry* buttonSetting(const std::string& name, const std::string& buttonLabel, std::function<void()> onClick) {
    Entry* e = new Entry();
    e->settingName = name;
    e->type = EntryType::Button;
    e->buttonLabel = buttonLabel;
    e->onChange = onClick;
    return e;
}

struct Section {
    Section() {}
    Section(const std::string& name, std::initializer_list<Entry*> entryList) : name(name) {
        for (auto* entry : entryList)
            entries.push_back(entry);
    }
    
    std::string name;
    std::vector<Entry*> entries;
    
    ~Section() {
        for (auto* entry : entries)
            delete entry;
    }
};

class GlobalSettings {
public:
    GlobalSettings(Application& app);
    ~GlobalSettings();

    void update();
    void render();
    void showWindow();
    void hideWindow();

    void addSection(const std::string& name, std::initializer_list<Entry*> entries);
    
    Section* createSection(const std::string& name);
    void addEntryToSection(Section* section, Entry* entry);
    
private:
    Application& app;
    sf::RenderWindow window;
    std::unique_ptr<UILO> ui;
    std::unordered_map<std::string, Section*> sections;
    std::vector<std::string> sectionOrder;
    
    struct SectionUI {
        Element* headerRow = nullptr;
        std::vector<Element*> settingRows;
        Element* spacer = nullptr;
    };
    std::unordered_map<std::string, SectionUI> sectionUIElements;
    
    std::string lastSearchQuery;
    
    void buildUI();
    void updateSettings();
    void applySearchFilter(const std::string& query);
};