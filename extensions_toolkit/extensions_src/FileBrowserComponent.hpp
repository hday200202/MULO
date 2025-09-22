#pragma once

#include "MULOComponent.hpp"
#include "FileTree.hpp"
#include "../../src/audio/VSTPluginManager.hpp"
#include "../../src/DebugConfig.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <unordered_map>

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
    
    bool doubleClick = false;
    sf::Clock doubleClickTimer;
    std::string lastClickedPath;
    std::string selectedItem;
    bool validSelection = false; // Flag to track if selection came from a valid file click
    
    // Store references to row elements for direct color manipulation
    std::unordered_map<std::string, Row*> rowElementsByPath;
    
    bool draggingItem = false;
    std::string draggingItemPath;
    sf::Vector2f dragStartPosition;
    sf::Vector2f currentMousePosition;
    sf::Image* dragIcon = nullptr;
    
    // Visual drag feedback using custom geometry
    sf::Texture dragIconTexture;
    std::unique_ptr<sf::Sprite> dragIconSprite;
    bool isDragIconVisible = false;

    // Main UI builder function
    void buildFileTreeUI();

    // Recursive functions to build the tree views
    void buildFileTreeUIRecursive(const FileTree& tree, int indentLevel);
    void buildVSTTreeUIRecursive(const FileTree& tree, int indentLevel);

    // Functions to handle tree node interactions
    void toggleTreeNodeByPath(const std::string& path);
    void toggleVSTTreeNodeByPath(const std::string& path);

    // Double-click handler for adding items to timeline
    bool handleDoubleClick(const std::string& path, std::function<void()> action);
    
    // Direct color manipulation for selection highlighting
    void updateSelectionColors();
    
    // Drag and drop functionality
    void startDrag(const std::string& path, const sf::Vector2f& mousePos);
    void updateDrag(const sf::Vector2f& mousePos);
    bool handleDrop(const sf::Vector2f& mousePos, std::function<void()> action);
    void cancelDrag();

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
    
    // Load favorites from config
    loadFavorites();
    
    std::string fileBrowserDir = app->readConfig<std::string>("fileBrowserDirectory");
    if (!fileBrowserDir.empty() && std::filesystem::is_directory(fileBrowserDir)) {
        fileTree.setRootDirectory(fileBrowserDir);
    }
    
    std::string vstDir = app->readConfig<std::string>("vstDirectory");
    if (!vstDir.empty() && std::filesystem::is_directory(vstDir)) {
        vstTree.setRootDirectory(vstDir);
    }
    
    buildFileTreeUI();
    
    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

void FileBrowserComponent::update() {
    // Handle drag and drop logic
    if (app->ui->isMouseDragging()) {
        if (!draggingItem && !selectedItem.empty() && validSelection) {
            // Start dragging the selected item only if it's a valid selection
            sf::Vector2f mousePos = app->ui->getMousePosition();
            startDrag(selectedItem, mousePos);
        } else if (draggingItem && !selectedItem.empty()) {
            // Update drag position only if we have a valid selected item
            sf::Vector2f mousePos = app->ui->getMousePosition();
            updateDrag(mousePos);
        }
    } else {
        // Mouse not dragging - clear any drag state
        if (draggingItem) {
            // Check for drop if we were dragging
            sf::Vector2f mousePos = app->ui->getMousePosition();
            
            // Determine action based on file type
            std::string ext = std::filesystem::path(draggingItemPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".vst" || ext == ".vst3") {
                // VST plugin
                handleDrop(mousePos, [this](){
                    app->addEffect(draggingItemPath);
                });
            } else {
                // Audio file
                handleDrop(mousePos, [this](){
                    juce::File sampleFile(draggingItemPath);
                    std::string trackName = sampleFile.getFileNameWithoutExtension().toStdString();
                    app->addTrack(trackName, draggingItemPath);
                });
            }
            
            // Always cancel drag after handling drop
            cancelDrag();
        }
    }
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
        
        app->writeConfig("fileBrowserDirectory", selectedDir);
    }
}

void FileBrowserComponent::browseForVSTDirectory() {
    std::string selectedDir = app->selectDirectory();
    if (!selectedDir.empty() && std::filesystem::is_directory(selectedDir)) {
        vstTree.setRootDirectory(selectedDir);
        vstTreeNeedsRebuild = true;
        
        app->writeConfig("vstDirectory", selectedDir);
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
        app->writeConfig("favoriteItems", favoriteItems);
    } catch (const std::exception& e) {}
}

void FileBrowserComponent::loadFavorites() {
    try {
        auto favoritesFromConfig = app->readConfig<std::vector<std::string>>("favoriteItems");
        
        favoriteItems.clear();
        for (const auto& path : favoritesFromConfig) {
            if (std::filesystem::exists(path)) {
                favoriteItems.push_back(path);
            }
        }
    } catch (const std::exception& e) {
        favoriteItems.clear();
    }
}

// --- UI Building ---

void FileBrowserComponent::buildFileTreeUI() {
    auto* scrollColumn = static_cast<ScrollableColumn*>(layout);
    if (!scrollColumn) return;

    scrollColumn->clear();
    
    // Clear stored row references when rebuilding UI
    rowElementsByPath.clear();

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
    auto favExpandIcon = image(
        Modifier()
            .setfixedHeight(25)
            .setfixedWidth(25)
            .align(Align::CENTER_Y)
            .setColor(app->resources.activeTheme->primary_text_color)
            .onLClick([this](){
                isFavoritesOpen = !isFavoritesOpen;
                favoritesTreeNeedsRebuild = true;
            }),
        isFavoritesOpen ? app->resources.openFolderIcon : app->resources.folderIcon,
        true
    );
    
    auto favRootTextElement = text(
        Modifier()
            .setfixedHeight(28)
            .setColor(app->resources.activeTheme->primary_text_color)
            .onLClick([this](){
                isFavoritesOpen = !isFavoritesOpen;
                favoritesTreeNeedsRebuild = true;
            }),
        "Favorites",
        app->resources.dejavuSansFont
    );

    scrollColumn->addElements({
        row(Modifier().setfixedHeight(28), contains{
            spacer(Modifier().setfixedWidth(20.f)),
            favExpandIcon,
            spacer(Modifier().setfixedWidth(8.f)),
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
            Image* iconElement = nullptr;
            
            if (ext == ".vst" || ext == ".vst3") {
                displayName = favName;
                iconElement = image(
                    Modifier()
                        .setfixedHeight(25)
                        .setfixedWidth(25)
                        .align(Align::CENTER_Y)
                        .setColor(app->resources.activeTheme->primary_text_color)
                        .onLClick([this, favPath](){
                            handleDoubleClick(favPath, [this, favPath](){
                                app->addEffect(favPath);
                            });
                        }),
                    app->resources.pluginFileIcon,
                    true
                );
                textModifier.onLClick([this, favPath](){
                    handleDoubleClick(favPath, [this, favPath](){
                        app->addEffect(favPath);
                    });
                });
            } else {
                displayName = favName;
                iconElement = image(
                    Modifier()
                        .setfixedHeight(25)
                        .setfixedWidth(25)
                        .align(Align::CENTER_Y)
                        .setColor(app->resources.activeTheme->primary_text_color)
                        .onLClick([this, favPath](){
                            handleDoubleClick(favPath, [this, favPath](){
                                juce::File sampleFile(favPath);
                                std::string trackName = sampleFile.getFileNameWithoutExtension().toStdString();
                                app->addTrack(trackName, favPath);
                            });
                        }),
                    app->resources.audioFileIcon,
                    true
                );
                textModifier.onLClick([this, favPath](){
                    handleDoubleClick(favPath, [this, favPath](){
                        juce::File sampleFile(favPath);
                        std::string trackName = sampleFile.getFileNameWithoutExtension().toStdString();
                        app->addTrack(trackName, favPath);
                    });
                });
            }
            
            textModifier.onRClick([this, favPath](){ 
                removeFavorite(favPath); 
            });
            
            auto textElement = text(textModifier, displayName, app->resources.dejavuSansFont);
            
            // Create row element and store reference for direct color manipulation
            auto rowElement = row(Modifier().setfixedHeight(28), contains{
                spacer(Modifier().setfixedWidth(40.f)),
                iconElement,
                spacer(Modifier().setfixedWidth(8.f)),
                textElement,
            });
            
            // Store row reference for later color updates
            rowElementsByPath[favPath] = rowElement;
            
            scrollColumn->addElements({
                rowElement,
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
        
        auto expandIcon = image(
            Modifier()
                .setfixedHeight(25)
                .setfixedWidth(25)
                .align(Align::CENTER_Y)
                .setColor(app->resources.activeTheme->primary_text_color)
                .onLClick([this](){
                    fileTree.toggleOpen();
                    fileTreeNeedsRebuild = true;
                }),
            fileTree.isOpen() ? app->resources.openFolderIcon : app->resources.folderIcon,
            true
        );

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
                expandIcon,
                spacer(Modifier().setfixedWidth(8.f)),
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
        
        auto vstExpandIcon = image(
            Modifier()
                .setfixedHeight(25)
                .setfixedWidth(25)
                .align(Align::CENTER_Y)
                .setColor(app->resources.activeTheme->primary_text_color)
                .onLClick([this](){
                    vstTree.toggleOpen();
                    vstTreeNeedsRebuild = true;
                }),
            vstTree.isOpen() ? app->resources.openFolderIcon : app->resources.folderIcon,
            true
        );

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
                vstExpandIcon,
                spacer(Modifier().setfixedWidth(8.f)),
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

    Image* iconElement = nullptr;

    if (tree.isDirectory()) {
        iconElement = image(
            Modifier()
                .setfixedHeight(25)
                .setfixedWidth(25)
                .align(Align::CENTER_Y)
                .setColor(app->resources.activeTheme->primary_text_color)
                .onLClick([this, filePath](){
                    toggleTreeNodeByPath(filePath);
                    fileTreeNeedsRebuild = true;
                }),
            tree.isOpen() ? app->resources.openFolderIcon : app->resources.folderIcon,
            true
        );
        textModifier.onLClick([this, filePath](){
            toggleTreeNodeByPath(filePath);
            fileTreeNeedsRebuild = true;
        });
    } else if (tree.isAudioFile()) {
        iconElement = image(
            Modifier()
                .setfixedHeight(25)
                .setfixedWidth(25)
                .align(Align::CENTER_Y)
                .setColor(app->resources.activeTheme->primary_text_color)
                .onLClick([this, filePath](){
                    handleDoubleClick(filePath, [this, filePath](){
                        juce::File sampleFile(filePath);
                        std::string trackName = sampleFile.getFileNameWithoutExtension().toStdString();
                        app->addTrack(trackName, filePath);
                    });
                }),
            app->resources.audioFileIcon,
            true
        );
        textModifier.onLClick([this, filePath](){
            handleDoubleClick(filePath, [this, filePath](){
                juce::File sampleFile(filePath);
                std::string trackName = sampleFile.getFileNameWithoutExtension().toStdString();
                app->addTrack(trackName, filePath);
            });
        });
        
        if (isFavorite) {
            textModifier.onRClick([this, filePath](){ removeFavorite(filePath); });
        } else {
            textModifier.onRClick([this, filePath](){ addFavorite(filePath); });
        }
    } else {
        // Default icon for unknown file types
        iconElement = image(
            Modifier()
                .setfixedHeight(25)
                .setfixedWidth(25)
                .align(Align::CENTER_Y)
                .setColor(app->resources.activeTheme->primary_text_color),
            app->resources.fileIcon,
            true
        );
    }

    auto textElement = text(textModifier, displayName, app->resources.dejavuSansFont);

    auto rowElement = row(Modifier().setfixedHeight(28), contains{
        spacer(Modifier().setfixedWidth(indent)),
        iconElement,
        spacer(Modifier().setfixedWidth(8.f)),
        textElement,
    });
    
    rowElementsByPath[filePath] = rowElement;

    scrollColumn->addElements({
        rowElement,
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

    Image* iconElement = nullptr;

    if (tree.isDirectory()) {
        iconElement = image(
            Modifier()
                .setfixedHeight(25)
                .setfixedWidth(25)
                .align(Align::CENTER_Y)
                .setColor(app->resources.activeTheme->primary_text_color)
                .onLClick([this, filePath](){
                    toggleVSTTreeNodeByPath(filePath);
                    vstTreeNeedsRebuild = true;
                }),
            tree.isOpen() ? app->resources.openFolderIcon : app->resources.folderIcon,
            true
        );
        textModifier.onLClick([this, filePath](){
            toggleVSTTreeNodeByPath(filePath);
            vstTreeNeedsRebuild = true;
        });
    } else if (tree.isVSTFile()) {
        iconElement = image(
            Modifier()
                .setfixedHeight(25)
                .setfixedWidth(25)
                .align(Align::CENTER_Y)
                .setColor(app->resources.activeTheme->primary_text_color)
                .onLClick([this, filePath](){
                    handleDoubleClick(filePath, [this, filePath](){
                        app->addEffect(filePath);
                    });
                }),
            app->resources.pluginFileIcon,
            true
        );
        textModifier.onLClick([this, filePath](){
            handleDoubleClick(filePath, [this, filePath](){
                app->addEffect(filePath);
            });
        });

        if (isFavorite) {
            textModifier.onRClick([this, filePath](){ removeFavorite(filePath); });
        } else {
            textModifier.onRClick([this, filePath](){ addFavorite(filePath); });
        }
    } else {
        // Default icon for unknown file types
        iconElement = image(
            Modifier()
                .setfixedHeight(25)
                .setfixedWidth(25)
                .align(Align::CENTER_Y)
                .setColor(app->resources.activeTheme->primary_text_color),
            app->resources.fileIcon,
            true
        );
    }

    auto textElement = text(textModifier, displayName, app->resources.dejavuSansFont);

    auto rowElement = row(Modifier().setfixedHeight(28), contains{
        spacer(Modifier().setfixedWidth(indent)),
        iconElement,
        spacer(Modifier().setfixedWidth(8.f)),
        textElement,
    });
    
    rowElementsByPath[filePath] = rowElement;

    scrollColumn->addElements({
        rowElement,
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

bool FileBrowserComponent::handleDoubleClick(const std::string& path, std::function<void()> action) {
    const sf::Time doubleClickTimeoutMs = sf::milliseconds(250);
    
    if (!doubleClick) {
        // First click
        doubleClick = true;
        lastClickedPath = path;
        selectedItem = path;
        validSelection = true;
        doubleClickTimer.restart();
        
        updateSelectionColors();
        
        return false;
    } else {
        // Second click
        if (lastClickedPath == path && doubleClickTimer.getElapsedTime() < doubleClickTimeoutMs) {
            doubleClick = false;
            lastClickedPath.clear();
            selectedItem.clear();
            validSelection = false;
            
            updateSelectionColors();
            
            action();
            return true;
        } else {
            doubleClick = true;
            lastClickedPath = path;
            selectedItem = path;
            validSelection = true;
            doubleClickTimer.restart();
            
            updateSelectionColors();
            
            return false;
        }
    }
}

void FileBrowserComponent::updateSelectionColors() {
    for (auto& [path, rowElement] : rowElementsByPath) {
        if (rowElement) {
            if (path == selectedItem) {
                rowElement->m_modifier.setColor(app->resources.activeTheme->foreground_color);
            } else {
                rowElement->m_modifier.setColor(sf::Color::Transparent);
            }
        }
    }
}

void FileBrowserComponent::startDrag(const std::string& path, const sf::Vector2f& mousePos) {
    draggingItem = true;
    draggingItemPath = path;
    dragStartPosition = mousePos;
    currentMousePosition = mousePos;
    
    // Determine the appropriate icon for the dragging item
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    sf::Image* iconImage;
    if (ext == ".vst" || ext == ".vst3") {
        dragIcon = &app->resources.pluginFileIcon;
        iconImage = &app->resources.pluginFileIcon;
    } else {
        dragIcon = &app->resources.audioFileIcon;
        iconImage = &app->resources.audioFileIcon;
    }
    
    sf::Image recoloredImage = *iconImage;
    
    // Recolor the image to primary text color
    sf::Color targetColor = app->resources.activeTheme->primary_text_color;
    for (unsigned int x = 0; x < recoloredImage.getSize().x; x++) {
        for (unsigned int y = 0; y < recoloredImage.getSize().y; y++) {
            sf::Color pixel = recoloredImage.getPixel({x, y});
            if (pixel.a > 0) {
                recoloredImage.setPixel({x, y}, sf::Color(targetColor.r, targetColor.g, targetColor.b, pixel.a));
            }
        }
    }
    
    if (dragIconTexture.loadFromImage(recoloredImage)) {
        dragIconSprite = std::make_unique<sf::Sprite>(dragIconTexture);
        dragIconSprite->setScale(sf::Vector2f(0.125f/2.f, 0.125f/2.f));
        
        float scaledWidth = dragIconTexture.getSize().x * 0.125f/2.f;
        float scaledHeight = dragIconTexture.getSize().y * 0.125f/2.f;

        dragIconSprite->setPosition(sf::Vector2f(mousePos.x + 360 - 32, mousePos.y));
        isDragIconVisible = true;
        
        std::vector<std::shared_ptr<sf::Drawable>> dragGeometry;
        dragGeometry.push_back(std::shared_ptr<sf::Drawable>(dragIconSprite.get(), [](sf::Drawable*){}));
        if (app && app->baseContainer) {
            app->baseContainer->setCustomGeometry(dragGeometry);
        }
    }
}

void FileBrowserComponent::updateDrag(const sf::Vector2f& mousePos) {
    if (draggingItem && isDragIconVisible && dragIconSprite) {
        currentMousePosition = mousePos;
        
        float scaledWidth = dragIconTexture.getSize().x * 0.125f/2.f;
        float scaledHeight = dragIconTexture.getSize().y * 0.125f/2.f;
        
        dragIconSprite->setPosition(sf::Vector2f(mousePos.x + 360 - 32, mousePos.y));
        
        std::vector<std::shared_ptr<sf::Drawable>> dragGeometry;
        dragGeometry.push_back(std::shared_ptr<sf::Drawable>(dragIconSprite.get(), [](sf::Drawable*){}));
        if (app && app->baseContainer) {
            app->baseContainer->setCustomGeometry(dragGeometry);
        }
    }
}

bool FileBrowserComponent::handleDrop(const sf::Vector2f& mousePos, std::function<void()> action) {
    if (!draggingItem) {
        return false;
    }
    
    auto timelineLayout = app->getComponentLayout("timeline");
    if (timelineLayout) {
        sf::FloatRect timelineBounds = timelineLayout->m_bounds.getGlobalBounds();
        if (timelineBounds.contains(mousePos)) {
            action();
            cancelDrag();
            return true;
        }
    }
    
    cancelDrag();
    return false;
}

void FileBrowserComponent::cancelDrag() {
    draggingItem = false;
    draggingItemPath.clear();
    dragIcon = nullptr;
    isDragIconVisible = false;
    
    selectedItem.clear();
    validSelection = false;
    
    updateSelectionColors();
    
    std::vector<std::shared_ptr<sf::Drawable>> emptyGeometry;
    if (app && app->baseContainer) {
        app->baseContainer->setCustomGeometry(emptyGeometry);
    }
    
    dragIconSprite.reset();
}

// Plugin interface for FileBrowserComponent
GET_INTERFACE

// Declare this class as a plugin
DECLARE_PLUGIN(FileBrowserComponent)
