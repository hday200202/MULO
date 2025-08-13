#pragma once

#include "MULOComponent.hpp"
#include "FileTree.hpp"
#include "../../src/audio/VSTPluginManager.hpp"
#include "../../src/DebugConfig.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

class FileBrowserComponent : public MULOComponent {
public:
    FileBrowserComponent();
    ~FileBrowserComponent() override;

    void init() override;
    void update() override;
    bool handleEvents() override;

private:
    // In-memory list for favorite items - just store paths as strings
    std::vector<std::string> favoriteItems; 
    bool isFavoritesOpen = true; // Manages expand/collapse state for favorites

    FileTree fileTree;
    FileTree vstTree;

    // Flags to trigger UI rebuilds
    bool favoritesTreeNeedsRebuild = false;
    bool fileTreeNeedsRebuild = false;
    bool vstTreeNeedsRebuild = false;
    
    // Main UI builder function
    void buildFileTreeUI();

    // Recursive functions to build the tree views
    void buildFileTreeUIRecursive(const FileTree& tree, int indentLevel);
    void buildVSTTreeUIRecursive(const FileTree& tree, int indentLevel);

    // Functions to handle tree node interactions
    void toggleTreeNodeByPath(const std::string& path);
    void toggleVSTTreeNodeByPath(const std::string& path);

    // In-memory functions to handle favorites
    void addFavorite(const std::string& path);
    void removeFavorite(const std::string& path);
    void saveFavorites();
    void loadFavorites();

    // Directory browsing functions
    void browseForDirectory();
    void browseForVSTDirectory();
};

#include "Application.hpp"
#include <algorithm> // For std::remove_if and std::find_if

FileBrowserComponent::FileBrowserComponent() {
    name = "file_browser";
}

FileBrowserComponent::~FileBrowserComponent() {}

void FileBrowserComponent::init() {
    if (app->mainContentRow)
        parentContainer = app->mainContentRow;

    relativeTo = "timeline";
    
    layout = scrollableColumn(
        Modifier()
            .align(Align::LEFT | Align::TOP)
            .setfixedWidth(360)
            .setColor(app->resources.activeTheme->track_color),
        contains{},
        "file_browser_scroll_column"
    );
    
    // Load favorites from file
    loadFavorites();
    
    if (!app->uiState.fileBrowserDirectory.empty() && 
        std::filesystem::is_directory(app->uiState.fileBrowserDirectory)) {
        fileTree.setRootDirectory(app->uiState.fileBrowserDirectory);
    }
    
    if (!app->uiState.vstDirecory.empty() && 
        std::filesystem::is_directory(app->uiState.vstDirecory)) {
        vstTree.setRootDirectory(app->uiState.vstDirecory);
    }
    
    buildFileTreeUI();
    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

void FileBrowserComponent::update() {
    // Not needed
}

bool FileBrowserComponent::handleEvents() {
    if (favoritesTreeNeedsRebuild || fileTreeNeedsRebuild || vstTreeNeedsRebuild) {
        buildFileTreeUI();
        favoritesTreeNeedsRebuild = false;
        fileTreeNeedsRebuild = false;
        vstTreeNeedsRebuild = false;
        forceUpdate = true;
    }

    return forceUpdate;
}

// --- Directory Browsing ---

void FileBrowserComponent::browseForDirectory() {
    std::string selectedDir = app->selectDirectory();
    if (!selectedDir.empty() && std::filesystem::is_directory(selectedDir)) {
        fileTree.setRootDirectory(selectedDir);
        fileTreeNeedsRebuild = true;
        
        app->uiState.fileBrowserDirectory = selectedDir;
        app->uiState.saveConfig();
    }
}

void FileBrowserComponent::browseForVSTDirectory() {
    std::string selectedDir = app->selectDirectory();
    if (!selectedDir.empty() && std::filesystem::is_directory(selectedDir)) {
        vstTree.setRootDirectory(selectedDir);
        vstTreeNeedsRebuild = true;
        
        app->uiState.vstDirecory = selectedDir;
        app->uiState.saveConfig();
    }
}

// --- Favorites Management (In-Memory) ---

void FileBrowserComponent::addFavorite(const std::string& path) {
    auto it = std::find(favoriteItems.begin(), favoriteItems.end(), path);

    if (it != favoriteItems.end()) {
        return; // Already a favorite
    }

    // Convert to unix-like path with forward slashes
    std::string unixPath = std::filesystem::path(path).generic_string();
    favoriteItems.push_back(unixPath);
    saveFavorites();
    favoritesTreeNeedsRebuild = true;
}

void FileBrowserComponent::removeFavorite(const std::string& path) {
    // Convert to unix-like path for comparison
    std::string unixPath = std::filesystem::path(path).generic_string();
    
    favoriteItems.erase(
        std::remove(favoriteItems.begin(), favoriteItems.end(), unixPath),
        favoriteItems.end()
    );
    saveFavorites();
    favoritesTreeNeedsRebuild = true;
}

void FileBrowserComponent::saveFavorites() {
    try {
        nlohmann::json favoritesJson = favoriteItems;
        
        std::ofstream file("favorites.json");
        if (file.is_open()) {
            file << favoritesJson.dump(2);
            file.close();
        }
    } catch (const std::exception& e) {
        // Silently handle save errors
    }
}

void FileBrowserComponent::loadFavorites() {
    try {
        std::ifstream file("favorites.json");
        if (file.is_open()) {
            nlohmann::json favoritesJson;
            file >> favoritesJson;
            file.close();
            
            favoriteItems.clear();
            for (const auto& path : favoritesJson) {
                if (path.is_string() && std::filesystem::exists(path.get<std::string>())) {
                    favoriteItems.push_back(path.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        // Silently handle load errors, start with empty favorites
        favoriteItems.clear();
    }
}

// --- UI Building ---

void FileBrowserComponent::buildFileTreeUI() {
    auto* scrollColumn = static_cast<ScrollableColumn*>(layout);
    if (!scrollColumn) return;

    scrollColumn->clear();

    // Favorites Section
    scrollColumn->addElements({
        spacer(Modifier().setfixedHeight(16)),
        row(Modifier().setfixedHeight(48),
        contains{
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            text(
                Modifier().align(Align::LEFT | Align::CENTER_Y).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "favorites",
                app->resources.dejavuSansFont
            ),
        }),
    });

    std::string favSymbol = isFavoritesOpen ? "[-] " : "[+] ";
    auto favRootTextElement = text(
        Modifier()
            .setfixedHeight(28)
            .setColor(app->resources.activeTheme->primary_text_color)
            .onLClick([this](){
                isFavoritesOpen = !isFavoritesOpen;
                favoritesTreeNeedsRebuild = true;
            }),
        favSymbol + "Favorites",
        app->resources.dejavuSansFont
    );

    scrollColumn->addElements({
        row(Modifier().setfixedHeight(28), contains{
            spacer(Modifier().setfixedWidth(20.f)),
            favRootTextElement,
        }),
        spacer(Modifier().setfixedHeight(12))
    });

    if (isFavoritesOpen) {
        for (const auto& favPath : favoriteItems) {
            std::string favName = std::filesystem::path(favPath).filename().string();
            
            if (favName.empty()) {
                size_t lastSlash = favPath.find_last_of("/\\");
                if (lastSlash != std::string::npos && lastSlash + 1 < favPath.length()) {
                    favName = favPath.substr(lastSlash + 1);
                } else {
                    favName = "Unknown File";
                }
            }
            
            std::string displayName;
            std::string ext = std::filesystem::path(favPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            Modifier textModifier = Modifier().setfixedHeight(28).setColor(app->resources.activeTheme->primary_text_color);
            
            if (ext == ".vst" || ext == ".vst3") {
                displayName = "[v] " + favName;
                textModifier.onLClick([this, favPath](){
                    app->addEffect(favPath);
                });
            } else {
                displayName = "[f] " + favName;
                textModifier.onLClick([this, favPath](){
                    juce::File sampleFile(favPath);
                    std::string trackName = sampleFile.getFileNameWithoutExtension().toStdString();
                    app->addTrack(trackName, favPath);
                });
            }
            
            textModifier.onRClick([this, favPath](){ 
                removeFavorite(favPath); 
            });
            
            auto textElement = text(textModifier, displayName, app->resources.dejavuSansFont);
            
            scrollColumn->addElements({
                row(Modifier().setfixedHeight(28), contains{
                    spacer(Modifier().setfixedWidth(40.f)),
                    textElement,
                }),
                spacer(Modifier().setfixedHeight(12))
            });
        }
    }

    // User Library Section
    scrollColumn->addElements({
        spacer(Modifier().setfixedHeight(16)),
        row(Modifier().setfixedHeight(48),
        contains{
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            text(
                Modifier().align(Align::LEFT | Align::CENTER_Y).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "user library",
                app->resources.dejavuSansFont
            ),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->alt_button_color)
                    .align(Align::RIGHT | Align::CENTER_Y)
                    .onLClick([this](){ browseForDirectory(); }),
                ButtonStyle::Pill,
                ". . .",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color,
                "select_directory"
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
        }),
        spacer(Modifier().setfixedHeight(16)),
    });

    if (!fileTree.getPath().empty()) {
        std::string displayName = fileTree.getName();
        std::string symbol = fileTree.isOpen() ? "[-] " : "[+] ";
        displayName = symbol + displayName;

        auto rootTextElement = text(
            Modifier()
                .setfixedHeight(28)
                .setColor(app->resources.activeTheme->primary_text_color)
                .onLClick([this](){
                    fileTree.toggleOpen();
                    fileTreeNeedsRebuild = true;
                }),
            displayName,
            app->resources.dejavuSansFont
        );

        scrollColumn->addElements({
            row(Modifier().setfixedHeight(28), contains{
                spacer(Modifier().setfixedWidth(20.f)),
                rootTextElement,
            }),
            spacer(Modifier().setfixedHeight(12))
        });

        if (fileTree.isOpen()) {
            for (const auto& subDir : fileTree.getSubDirectories()) {
                buildFileTreeUIRecursive(*subDir, 2);
            }
            for (const auto& file : fileTree.getFiles()) {
                buildFileTreeUIRecursive(*file, 2);
            }
        }
    }

    // VST Plugins Section
    scrollColumn->addElements({
        spacer(Modifier().setfixedHeight(24)),
        row(Modifier().setfixedHeight(48),
        contains{
            spacer(Modifier().setfixedWidth(16).align(Align::LEFT)),
            text(
                Modifier().align(Align::LEFT | Align::CENTER_Y).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "vst3 plugins",
                app->resources.dejavuSansFont
            ),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->alt_button_color)
                    .align(Align::RIGHT | Align::CENTER_Y)
                    .onLClick([this](){ browseForVSTDirectory(); }),
                ButtonStyle::Pill,
                ". . .",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color,
                "select_vst_directory"
            ),
            spacer(Modifier().setfixedWidth(16).align(Align::RIGHT)),
        }),
        spacer(Modifier().setfixedHeight(16)),
    });

    if (!vstTree.getPath().empty()) {
        std::string displayName = vstTree.getName();
        std::string symbol = vstTree.isOpen() ? "[-] " : "[+] ";
        displayName = symbol + displayName;

        auto vstRootTextElement = text(
            Modifier()
                .setfixedHeight(28)
                .setColor(app->resources.activeTheme->primary_text_color)
                .onLClick([this](){
                    vstTree.toggleOpen();
                    vstTreeNeedsRebuild = true;
                }),
            displayName,
            app->resources.dejavuSansFont
        );

        scrollColumn->addElements({
            row(Modifier().setfixedHeight(28), contains{
                spacer(Modifier().setfixedWidth(20.f)),
                vstRootTextElement,
            }),
            spacer(Modifier().setfixedHeight(12))
        });

        if (vstTree.isOpen()) {
            for (const auto& subDir : vstTree.getSubDirectories()) {
                buildVSTTreeUIRecursive(*subDir, 2);
            }
            for (const auto& file : vstTree.getFiles()) {
                buildVSTTreeUIRecursive(*file, 2);
            }
        }
    }
}

void FileBrowserComponent::buildFileTreeUIRecursive(const FileTree& tree, int indentLevel) {
    auto* scrollColumn = static_cast<ScrollableColumn*>(layout);
    if (!scrollColumn) return;

    float indent = indentLevel * 20.f;
    std::string displayName = tree.getName();
    Modifier textModifier = Modifier().setfixedHeight(28).setColor(app->resources.activeTheme->primary_text_color);
    std::string filePath = tree.getPath();
    
    // Convert to unix-like path for comparison with favorites
    std::string unixPath = std::filesystem::path(filePath).generic_string();
    auto favIt = std::find(favoriteItems.begin(), favoriteItems.end(), unixPath);
    bool isFavorite = favIt != favoriteItems.end();

    if (tree.isDirectory()) {
        std::string symbol = tree.isOpen() ? "[-] " : "[+] ";
        displayName = symbol + displayName;
        textModifier.onLClick([this, filePath](){
            toggleTreeNodeByPath(filePath);
            fileTreeNeedsRebuild = true;
        });
    } else if (tree.isAudioFile()) {
        displayName = "[f] " + displayName;
        textModifier.onLClick([this, filePath](){
            juce::File sampleFile(filePath);
            std::string trackName = sampleFile.getFileNameWithoutExtension().toStdString();
            app->addTrack(trackName, filePath);
        });
        
        if (isFavorite) {
            textModifier.onRClick([this, filePath](){ removeFavorite(filePath); });
        } else {
            textModifier.onRClick([this, filePath](){ addFavorite(filePath); });
        }
    }

    auto textElement = text(textModifier, displayName, app->resources.dejavuSansFont);

    scrollColumn->addElements({
        row(Modifier().setfixedHeight(28), contains{
            spacer(Modifier().setfixedWidth(indent)),
            textElement,
        }),
        spacer(Modifier().setfixedHeight(12))
    });

    if (tree.isDirectory() && tree.isOpen()) {
        for (const auto& subDir : tree.getSubDirectories()) {
            buildFileTreeUIRecursive(*subDir, indentLevel + 1);
        }
        for (const auto& file : tree.getFiles()) {
            buildFileTreeUIRecursive(*file, indentLevel + 1);
        }
    }
}


void FileBrowserComponent::buildVSTTreeUIRecursive(const FileTree& tree, int indentLevel) {
    auto* scrollColumn = static_cast<ScrollableColumn*>(layout);
    if (!scrollColumn) return;

    float indent = indentLevel * 20.f;
    std::string displayName = tree.getName();
    Modifier textModifier = Modifier().setfixedHeight(28).setColor(app->resources.activeTheme->primary_text_color);
    std::string filePath = tree.getPath();

    // Convert to unix-like path for comparison with favorites
    std::string unixPath = std::filesystem::path(filePath).generic_string();
    auto favIt = std::find(favoriteItems.begin(), favoriteItems.end(), unixPath);
    bool isFavorite = favIt != favoriteItems.end();

    if (tree.isDirectory()) {
        std::string symbol = tree.isOpen() ? "[-] " : "[+] ";
        displayName = symbol + displayName;
        textModifier.onLClick([this, filePath](){
            toggleVSTTreeNodeByPath(filePath);
            vstTreeNeedsRebuild = true;
        });
    } else if (tree.isVSTFile()) {
        displayName = "[v] " + displayName;
        textModifier.onLClick([this, filePath](){
            app->addEffect(filePath);
        });

        if (isFavorite) {
            textModifier.onRClick([this, filePath](){ removeFavorite(filePath); });
        } else {
            textModifier.onRClick([this, filePath](){ addFavorite(filePath); });
        }
    }

    auto textElement = text(textModifier, displayName, app->resources.dejavuSansFont);

    scrollColumn->addElements({
        row(Modifier().setfixedHeight(28), contains{
            spacer(Modifier().setfixedWidth(indent)),
            textElement,
        }),
        spacer(Modifier().setfixedHeight(12))
    });

    if (tree.isDirectory() && tree.isOpen()) {
        for (const auto& subDir : tree.getSubDirectories()) {
            buildVSTTreeUIRecursive(*subDir, indentLevel + 1);
        }
        for (const auto& file : tree.getFiles()) {
            buildVSTTreeUIRecursive(*file, indentLevel + 1);
        }
    }
}

void FileBrowserComponent::toggleTreeNodeByPath(const std::string& path) {
    std::function<bool(FileTree&)> findAndToggle = 
        [&](FileTree& node) -> bool {
        if (node.getPath() == path) {
            node.toggleOpen();
            return true;
        }
        for (auto& subDir : node.getSubDirectories()) {
            if (findAndToggle(*subDir)) return true;
        }
        return false;
    };
    findAndToggle(fileTree);
}

void FileBrowserComponent::toggleVSTTreeNodeByPath(const std::string& path) {
    std::function<bool(FileTree&)> findAndToggle = 
        [&](FileTree& node) -> bool {
        if (node.getPath() == path) {
            node.toggleOpen();
            return true;
        }
        for (auto& subDir : node.getSubDirectories()) {
            if (findAndToggle(*subDir)) return true;
        }
        return false;
    };
    findAndToggle(vstTree);
}

// Plugin interface for FileBrowserComponent
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(FileBrowserComponent)
