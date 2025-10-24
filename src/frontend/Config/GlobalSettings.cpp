#include "GlobalSettings.hpp"
#include "../Application.hpp"

GlobalSettings::GlobalSettings(Application& app) : app(app) {
}

GlobalSettings::~GlobalSettings() {
    for (auto& pair : sections)
        delete pair.second;
}

void GlobalSettings::update() {
    if (app.uiState.settingsShown && !window.isOpen())
        showWindow();
    
    if (!app.uiState.settingsShown && window.isOpen())
        hideWindow();
    
    if (window.isOpen() && ui) {
        ui->forceUpdate();        
        updateSettings();
        if (window.isOpen())
            render();
    }
}

void GlobalSettings::render() {
    if (window.isOpen() && ui && ui->windowShouldUpdate()) {
        window.clear(app.resources.activeTheme->foreground_color);
        ui->render();
        window.display();
    }
}

void GlobalSettings::showWindow() {
    if (!window.isOpen()) {
        sf::VideoMode windowMode = sf::VideoMode::getDesktopMode();
        windowMode.size.x = windowMode.size.x / 3.f;
        windowMode.size.y = windowMode.size.y / 2.f;
        window.create(windowMode, "Global Settings", sf::Style::None);
        
        sf::Vector2i mainPos = app.getWindow().getPosition();
        sf::Vector2u mainSize = app.getWindow().getSize();
        window.setPosition(sf::Vector2i(
            mainPos.x + (mainSize.x - windowMode.size.x) / 2,
            mainPos.y + (mainSize.y - windowMode.size.y) / 2
        ));
        
        sf::View view;
        view.setSize(static_cast<sf::Vector2f>(windowMode.size));
        view.setCenter({view.getSize().x / 2.0f, view.getSize().y / 2.0f});
        window.setView(view);
        
        ui = std::make_unique<UILO>(window, view);
        
        buildUI();
        
        app.ui->setInputBlocked(true);
    }
}

void GlobalSettings::hideWindow() {
    if (window.isOpen()) {
        window.close();
        ui.reset();
        app.ui->setInputBlocked(false);
    }
}

void GlobalSettings::addSection(const std::string& name, std::initializer_list<mulo::Entry*> entries) {
    if (sections.find(name) != sections.end()) {
        delete sections[name];
        sections.erase(name);
        auto it = std::find(sectionOrder.begin(), sectionOrder.end(), name);
        if (it != sectionOrder.end()) {
            sectionOrder.erase(it);
        }
    }
    
    Section* section = new Section(name, entries);
    sections[name] = section;
    sectionOrder.push_back(name);
}

Section* GlobalSettings::createSection(const std::string& name) {
    if (sections.find(name) != sections.end())
        return sections[name];
    
    Section* section = new Section();
    section->name = name;
    sections[name] = section;
    sectionOrder.push_back(name);
    return section;
}

void GlobalSettings::addEntryToSection(Section* section,  mulo::Entry* entry) {
    if (!section) {
        delete entry;
        return;
    }
    
    for (size_t i = 0; i < section->entries.size(); ++i) {
        if (section->entries[i]->settingName == entry->settingName) {
            delete section->entries[i];
            section->entries[i] = entry;
            return;
        }
    }
    
    section->entries.push_back(entry);
}

void GlobalSettings::updateSettings() {
    if (!ui || !window.isOpen()) return;
    
    if (auto* searchBox = ui->getTextBox("search_box")) {
        std::string currentSearch = searchBox->getText();
        if (currentSearch != lastSearchQuery) {
            lastSearchQuery = currentSearch;
            applySearchFilter(currentSearch);
        }
    }
    
    for (const auto& sectionName : sectionOrder) {
        if (!window.isOpen()) return;
        
        Section* section = sections[sectionName];
        for (auto* entry : section->entries) {
            std::string elemId;
            
            switch (entry->type) {
                case EntryType::Slider: {
                    elemId = "slider_" + entry->settingName;
                    if (auto* slider = ui->getSlider(elemId)) {
                        float* paramPtr = static_cast<float*>(entry->paramPtr);
                        if (paramPtr) {
                            float newValue = slider->getValue();
                            if (*paramPtr != newValue) {
                                *paramPtr = newValue;
                                if (entry->onChange) entry->onChange();
                            }
                        }
                    }
                    break;
                }
                
                case EntryType::Dropdown: {
                    elemId = "dropdown_" + entry->settingName;
                    if (auto* dropdown = ui->getDropdown(elemId)) {
                        std::string* paramPtr = static_cast<std::string*>(entry->paramPtr);
                        if (paramPtr) {
                            std::string newValue = dropdown->getSelected();
                            if (*paramPtr != newValue) {
                                *paramPtr = newValue;
                                if (entry->onChange) entry->onChange();
                            }
                        }
                    }
                    break;
                }
                
                case EntryType::TextBox: {
                    elemId = "textbox_" + entry->settingName;
                    if (auto* textbox = ui->getTextBox(elemId)) {
                        std::string* paramPtr = static_cast<std::string*>(entry->paramPtr);
                        if (paramPtr) {
                            std::string newValue = textbox->getText();
                            if (*paramPtr != newValue) {
                                *paramPtr = newValue;
                                if (entry->onChange) entry->onChange();
                            }
                        }
                    }
                    break;
                }
                
                case EntryType::Toggle: {
                    elemId = "toggle_" + entry->settingName;
                    if (auto* toggleBtn = ui->getButton(elemId)) {
                        bool* paramPtr = static_cast<bool*>(entry->paramPtr);
                        if (paramPtr) {
                            bool currentValue = *paramPtr;
                            toggleBtn->setText(currentValue ? "ON" : "OFF");
                            toggleBtn->m_modifier.setColor(currentValue ? app.resources.activeTheme->button_color : app.resources.activeTheme->middle_color);
                        }
                    }
                    break;
                }
                case EntryType::Button: break;
            }
        }
    }
}

void GlobalSettings::buildUI() {
    if (!ui) return;
    
    sectionUIElements.clear();
    std::vector<Element*> scrollContents;
    
    for (const auto& sectionName : sectionOrder) {
        Section* section = sections[sectionName];
        SectionUI& sectionUI = sectionUIElements[sectionName];
        
        sectionUI.headerRow = row(Modifier().setfixedHeight(64), contains{
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            text(
                Modifier()
                    .setfixedHeight(48)
                    .setColor(app.resources.activeTheme->primary_text_color)
                    .align(Align::CENTER_Y),
                section->name,
                app.resources.dejavuSansFont
            ),
        });
        scrollContents.push_back(sectionUI.headerRow);
        
        for (auto* entry : section->entries) {
            Element* settingRow = nullptr;
            
            switch (entry->type) {
                case EntryType::Slider: {
                    float* paramPtr = static_cast<float*>(entry->paramPtr);
                    settingRow = row(Modifier().setfixedHeight(64), contains{
                        spacer(Modifier().setfixedWidth(64).align(Align::LEFT)),
                        text(
                            Modifier()
                                .setfixedHeight(32)
                                .setColor(app.resources.activeTheme->primary_text_color)
                                .align(Align::CENTER_Y),
                            entry->settingName,
                            app.resources.dejavuSansFont
                        ),
                        horizontalSlider(
                            Modifier()
                                .setfixedWidth(200)
                                .setfixedHeight(40)
                                .align(Align::RIGHT | Align::CENTER_Y),
                            app.resources.activeTheme->slider_knob_color,
                            app.resources.activeTheme->slider_bar_color,
                            paramPtr ? *paramPtr : entry->defaultValue,
                            "slider_" + entry->settingName
                        ),
                        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                    });
                    break;
                }
                
                case EntryType::Dropdown: {
                    std::string* paramPtr = static_cast<std::string*>(entry->paramPtr);
                    std::string defaultVal = paramPtr ? *paramPtr : entry->stringDefault;
                    
                    settingRow = row(Modifier().setfixedHeight(64), contains{
                        spacer(Modifier().setfixedWidth(64).align(Align::LEFT)),
                        text(
                            Modifier()
                                .setfixedHeight(32)
                                .setColor(app.resources.activeTheme->primary_text_color)
                                .align(Align::CENTER_Y),
                            entry->settingName,
                            app.resources.dejavuSansFont
                        ),
                        dropdown(
                            Modifier()
                                .setfixedWidth(200)
                                .setfixedHeight(40)
                                .setColor(app.resources.activeTheme->alt_button_color)
                                .align(Align::RIGHT | Align::CENTER_Y),
                            defaultVal,
                            entry->options,
                            app.resources.dejavuSansFont,
                            app.resources.activeTheme->primary_text_color,
                            app.resources.activeTheme->alt_button_color,
                            "dropdown_" + entry->settingName
                        ),
                        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                    });
                    break;
                }
                
                case EntryType::Toggle: {
                    bool* paramPtr = static_cast<bool*>(entry->paramPtr);
                    bool currentValue = paramPtr ? *paramPtr : entry->boolDefault;
                    
                    settingRow = row(Modifier().setfixedHeight(64), contains{
                        spacer(Modifier().setfixedWidth(64).align(Align::LEFT)),
                        text(
                            Modifier()
                                .setfixedHeight(32)
                                .setColor(app.resources.activeTheme->primary_text_color)
                                .align(Align::CENTER_Y),
                            entry->settingName,
                            app.resources.dejavuSansFont
                        ),
                        button(
                            Modifier()
                                .setfixedHeight(40)
                                .setfixedWidth(80)
                                .setColor(currentValue ? app.resources.activeTheme->button_color : app.resources.activeTheme->middle_color)
                                .align(Align::CENTER_Y | Align::RIGHT)
                                .onLClick([paramPtr, entry]() {
                                    if (paramPtr) {
                                        *paramPtr = !(*paramPtr);
                                        if (entry->onChange) entry->onChange();
                                    }
                                }),
                            ButtonStyle::Pill,
                            currentValue ? "ON" : "OFF",
                            app.resources.dejavuSansFont,
                            app.resources.activeTheme->secondary_text_color,
                            "toggle_" + entry->settingName
                        ),
                        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                    });
                    break;
                }
                
                case EntryType::TextBox: {
                    std::string* paramPtr = static_cast<std::string*>(entry->paramPtr);
                    std::string defaultVal = paramPtr ? *paramPtr : entry->stringDefault;
                    
                    settingRow = row(Modifier().setfixedHeight(64), contains{
                        spacer(Modifier().setfixedWidth(64).align(Align::LEFT)),
                        text(
                            Modifier()
                                .setfixedHeight(32)
                                .setColor(app.resources.activeTheme->primary_text_color)
                                .align(Align::CENTER_Y),
                            entry->settingName,
                            app.resources.dejavuSansFont
                        ),
                        textBox(
                            Modifier()
                                .setfixedWidth(200)
                                .setfixedHeight(40)
                                .setColor(sf::Color::White)
                                .align(Align::RIGHT | Align::CENTER_Y),
                            TBStyle::Pill,
                            app.resources.dejavuSansFont,
                            defaultVal,
                            app.resources.activeTheme->foreground_color,
                            app.resources.activeTheme->button_color,
                            "textbox_" + entry->settingName
                        ),
                        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                    });
                    break;
                }
                
                case EntryType::Button: {
                    settingRow = row(Modifier().setfixedHeight(64), contains{
                        spacer(Modifier().setfixedWidth(64).align(Align::LEFT)),
                        text(
                            Modifier()
                                .setfixedHeight(32)
                                .setColor(app.resources.activeTheme->primary_text_color)
                                .align(Align::CENTER_Y),
                            entry->settingName,
                            app.resources.dejavuSansFont
                        ),
                        button(
                            Modifier()
                                .setfixedHeight(40)
                                .setfixedWidth(200)
                                .setColor(app.resources.activeTheme->button_color)
                                .align(Align::CENTER_Y | Align::RIGHT)
                                .onLClick([entry]() {
                                    if (entry->onChange) entry->onChange();
                                }),
                            ButtonStyle::Pill,
                            entry->buttonLabel,
                            app.resources.dejavuSansFont,
                            app.resources.activeTheme->secondary_text_color,
                            "button_" + entry->settingName
                        ),
                        spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
                    });
                    break;
                }
            }
            
            if (settingRow) {
                sectionUI.settingRows.push_back(settingRow);
                scrollContents.push_back(settingRow);
            }
        }
        
        sectionUI.spacer = spacer(Modifier().setfixedHeight(32));
        scrollContents.push_back(sectionUI.spacer);
    }
    
    auto* scrollColumn = scrollableColumn(
        Modifier().setColor(app.resources.activeTheme->foreground_color),
        {}
    );
    scrollColumn->setScrollSpeed(40.f);
    
    for (auto* elem : scrollContents)
        scrollColumn->addElement(elem);
    
    auto* layout = column(
        Modifier(),
        contains{
            row(Modifier().setfixedHeight(64).setColor(app.resources.activeTheme->foreground_color), contains{
                spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                textBox(
                    Modifier()
                        .setfixedHeight(40)
                        .setWidth(1.0f)
                        .setColor(sf::Color::White)
                        .align(Align::CENTER_Y),
                    TBStyle::Pill,
                    app.resources.dejavuSansFont,
                    "Search settings...",
                    app.resources.activeTheme->foreground_color,
                    app.resources.activeTheme->button_color,
                    "search_box"
                ),
                spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
            }),
            scrollColumn,
            row(Modifier().setfixedHeight(64).setColor(app.resources.activeTheme->foreground_color), contains{
                spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
                button(
                    Modifier()
                        .setfixedHeight(48)
                        .setfixedWidth(96)
                        .setColor(app.resources.activeTheme->button_color)
                        .align(Align::CENTER_Y | Align::CENTER_X)
                        .onLClick([this]() {
                            app.uiState.settingsShown = false;
                        }),
                    ButtonStyle::Pill,
                    "close",
                    app.resources.dejavuSansFont,
                    app.resources.activeTheme->secondary_text_color,
                    "close_button"
                ),
                spacer(Modifier().setfixedWidth(16).align(Align::RIGHT))
            })
        }
    );
    
    ui->addPage(page({layout}), "global_settings");
    ui->forceUpdate();
}

void GlobalSettings::applySearchFilter(const std::string& query) {
    if (!ui) return;
    
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    bool showAll = lowerQuery.empty() || lowerQuery == "search settings...";
    
    for (const auto& sectionName : sectionOrder) {
        auto it = sectionUIElements.find(sectionName);
        if (it == sectionUIElements.end()) continue;
        
        SectionUI& sectionUI = it->second;
        Section* section = sections[sectionName];
        
        std::string lowerSectionName = sectionName;
        std::transform(lowerSectionName.begin(), lowerSectionName.end(), lowerSectionName.begin(), ::tolower);
        
        bool sectionMatches = showAll || lowerSectionName.find(lowerQuery) != std::string::npos;
        bool anySettingVisible = false;
        
        for (size_t i = 0; i < sectionUI.settingRows.size() && i < section->entries.size(); ++i) {
            Element* settingRow = sectionUI.settingRows[i];
             mulo::Entry* entry = section->entries[i];
            
            std::string lowerSettingName = entry->settingName;
            std::transform(lowerSettingName.begin(), lowerSettingName.end(), lowerSettingName.begin(), ::tolower);
            
            bool settingMatches = sectionMatches || lowerSettingName.find(lowerQuery) != std::string::npos;
            
            if (settingRow) {
                settingRow->m_modifier.setVisible(settingMatches);
                if (settingMatches) anySettingVisible = true;
            }
        }
        
        if (sectionUI.headerRow)
            sectionUI.headerRow->m_modifier.setVisible(showAll || anySettingVisible);
        
        if (sectionUI.spacer)
            sectionUI.spacer->m_modifier.setVisible(showAll || anySettingVisible);
    }
}
