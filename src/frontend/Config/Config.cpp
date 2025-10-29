#include "Application.hpp"
#include <fstream>
#include <iostream>

void Application::saveConfig() {
    try {
        std::string configPath = exeDirectory + "/config.json";
        std::ofstream file(configPath);
        if (!file.is_open()) {
            DEBUG_PRINT("Failed to open config file for writing: " << configPath);
            return;
        }
        
        file << config.dump(2);
        file.close();        
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error saving config: " << e.what());
    }
}

void Application::loadConfig() {
    try {
        std::string configPath = exeDirectory + "/config.json";
        std::ifstream file(configPath);
        
        if (!file.is_open()) {
            DEBUG_PRINT("Config file not found, using defaults: " << configPath);
            return;
        }

        file >> config;
        file.close();
        
        uiState.fileBrowserDirectory = readConfig<std::string>("fileBrowserDirectory", "");
        uiState.vstDirecory = readConfig<std::string>("vstDirectory", "");
        uiState.vstDirectories = readConfig<std::vector<std::string>>("vstDirectories", std::vector<std::string>());
        uiState.saveDirectory = readConfig<std::string>("saveDirectory", "");
        uiState.selectedTheme = readConfig<std::string>("selectedTheme", "Dark");
        
        DEBUG_PRINT("Configuration loaded from: " << configPath);
    } catch (const nlohmann::json::parse_error& e) {
        DEBUG_PRINT("JSON parse error loading config: " << e.what());
    } catch (const std::exception& e) {
        DEBUG_PRINT("Error loading config: " << e.what());
    }
}

void Application::saveLayoutConfig() {
    nlohmann::json j;
    for (const auto& [name, layout] : componentLayouts) {
        std::string parentName = layout.parent ? layout.parent->m_name : "";
        
        // Save the actual index in the parent container
        int index = -1;
        if (layout.parent && muloComponents.find(name) != muloComponents.end()) {
            auto& component = muloComponents[name];
            if (component->getLayout()) {
                index = layout.parent->getElementIndex(component->getLayout());
            }
        }
        
        j[name] = {
            {"parent", parentName},
            {"alignment", static_cast<int>(layout.alignment)},
            {"relativeTo", layout.relativeTo},
            {"index", index}
        };
    }
    std::string path = exeDirectory + "/layout.json";
    std::ofstream ofs(path);
    if (ofs.is_open()) {
        ofs << j.dump(4);
        ofs.close();
        std::cout << "Layout saved to: " << path << std::endl;
    } else {
        std::cerr << "Failed to open layout.json for writing: " << path << std::endl;
    }
}

void Application::loadLayoutConfig() {
    std::string path = exeDirectory + "/layout.json";
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "Failed to open layout.json for reading: " << path << std::endl;
        return;
    }
    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing layout.json: " << e.what() << std::endl;
        return;
    }

    std::unordered_map<std::string, Container*> containerMap;
    for (auto& [name, component] : muloComponents) {
        if (auto* c = dynamic_cast<Container*>(component.get())) {
            containerMap[name] = c;
        }
    }

    for (auto& [name, layoutData] : j.items()) {
        if (muloComponents.find(name) == muloComponents.end()) continue;
        auto& component = muloComponents[name];
        auto& layout = componentLayouts[name];

        std::string parentName = layoutData.value("parent", "");
        Container* parent = nullptr;
        if (!parentName.empty() && containerMap.count(parentName)) {
            parent = containerMap[parentName];
            component->setParentContainer(parent);
            layout.parent = parent;
            std::cout << "[LAYOUT] Set parent for " << name << " to " << parentName << std::endl;
        }

        int alignInt = layoutData.value("alignment", static_cast<int>(Align::NONE));
        Align align = static_cast<Align>(alignInt);
        if (component->getLayout()) {
            component->getLayout()->m_modifier.align(align);
            std::cout << "[LAYOUT] Set alignment for " << name << " to " << alignInt << std::endl;
        }
        layout.alignment = align;

        std::string relTo = layoutData.value("relativeTo", "");
        component->setRelativeTo(relTo);
        layout.relativeTo = relTo;
        if (!relTo.empty()) {
            std::cout << "[LAYOUT] Set relativeTo for " << name << " to " << relTo << std::endl;
        }
    }
    
    // Build a list of components with their target indices
    std::vector<std::tuple<std::string, Element*, Container*, int>> reorderList;
    
    for (auto& [name, layoutData] : j.items()) {
        if (muloComponents.find(name) == muloComponents.end()) continue;
        auto& component = muloComponents[name];
        
        int targetIndex = layoutData.value("index", -1);
        Container* parent = component->getParentContainer();
        Element* elem = component->getLayout();
        
        if (targetIndex != -1 && parent && elem) {
            reorderList.push_back({name, elem, parent, targetIndex});
        }
    }
    
    // Sort by target index and reorder
    std::sort(reorderList.begin(), reorderList.end(), 
              [](const auto& a, const auto& b) { return std::get<3>(a) < std::get<3>(b); });
    
    // Group by parent and reorder within each parent
    std::unordered_map<Container*, std::vector<std::tuple<std::string, Element*, int>>> byParent;
    for (const auto& [name, elem, parent, targetIndex] : reorderList) {
        byParent[parent].push_back({name, elem, targetIndex});
    }
    
    for (auto& [parent, elements] : byParent) {
        // Remove all elements that need reordering
        for (const auto& [name, elem, targetIndex] : elements) {
            parent->removeElement(elem);
        }
        // Re-insert them in the correct order
        for (const auto& [name, elem, targetIndex] : elements) {
            parent->insertElementAt(elem, targetIndex);
            std::cout << "[LAYOUT] Reordered " << name << " to index " << targetIndex << std::endl;
        }
    }
    
    std::cout << "Layout loaded from: " << path << std::endl;
    
    // Force UI update to apply changes
    if (ui) {
        ui->forceUpdate();
    }
}

void Application::syncUIStateToConfig() {
    writeConfig("fileBrowserDirectory", uiState.fileBrowserDirectory);
    writeConfig("vstDirectory", uiState.vstDirecory);
    writeConfig("vstDirectories", uiState.vstDirectories);
    writeConfig("saveDirectory", uiState.saveDirectory);
    writeConfig("selectedTheme", uiState.selectedTheme);
    writeConfig("sampleRate", uiState.sampleRate);
    writeConfig("autoSaveIntervalSeconds", uiState.autoSaveIntervalSeconds);
    writeConfig("enableAutoVSTScan", uiState.enableAutoVSTScan);
}

template<typename T>
void Application::writeConfig(const std::string& key, const T& value) {
    config[key] = value;
    saveConfig();
}

template<typename T>
T Application::readConfig(const std::string& key, const T& defaultValue) const {
    if (config.contains(key)) {
        try {
            return config[key].get<T>();
        } catch (const std::exception& e) {
            return defaultValue;
        }
    }
    return defaultValue;
}

// Explicit template instantiations for common types
template void Application::writeConfig<std::string>(const std::string&, const std::string&);
template void Application::writeConfig<int>(const std::string&, const int&);
template void Application::writeConfig<float>(const std::string&, const float&);
template void Application::writeConfig<double>(const std::string&, const double&);
template void Application::writeConfig<bool>(const std::string&, const bool&);
template void Application::writeConfig<std::vector<std::string>>(const std::string&, const std::vector<std::string>&);

template std::string Application::readConfig<std::string>(const std::string&, const std::string&) const;
template int Application::readConfig<int>(const std::string&, const int&) const;
template float Application::readConfig<float>(const std::string&, const float&) const;
template double Application::readConfig<double>(const std::string&, const double&) const;
template bool Application::readConfig<bool>(const std::string&, const bool&) const;
template std::vector<std::string> Application::readConfig<std::vector<std::string>>(const std::string&, const std::vector<std::string>&) const;
