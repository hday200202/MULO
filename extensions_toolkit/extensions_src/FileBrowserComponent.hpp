
#pragma once

#include "MULOComponent.hpp"
#include "FileTree.hpp"
#include "../../src/audio/VSTPluginManager.hpp"
#include "../../src/DebugConfig.hpp"

class FileBrowserComponent : public MULOComponent {
public:
    FileBrowserComponent();
    ~FileBrowserComponent() override;

    void init() override;
    void update() override;
    bool handleEvents() override;

private:
    FileTree fileTree;
    FileTree vstTree;
    bool fileTreeNeedsRebuild = false;
    bool vstTreeNeedsRebuild = false;
    
    void buildFileTreeUI();
    void buildFileTreeUIRecursive(const FileTree& tree, int indentLevel);
    void buildVSTTreeUIRecursive(const FileTree& tree, int indentLevel);
    void toggleTreeNodeByPath(const std::string& path);
    void toggleVSTTreeNodeByPath(const std::string& path);
    void browseForDirectory();
    void browseForVSTDirectory();
};

#include "Application.hpp"

FileBrowserComponent::FileBrowserComponent() {
    name = "file_browser";
}

FileBrowserComponent::~FileBrowserComponent() {}

void FileBrowserComponent::init() {
    if (app->mainContentRow)
        parentContainer = app->mainContentRow;
    layout = scrollableColumn(
        Modifier()
            .align(Align::LEFT | Align::TOP)
            .setfixedWidth(360)
            .setColor(app->resources.activeTheme->track_color),
        contains{},
        "file_browser_scroll_column"
    );
    
    if (!app->uiState.fileBrowserDirectory.empty() && 
        std::filesystem::is_directory(app->uiState.fileBrowserDirectory)) {
        fileTree.setRootDirectory(app->uiState.fileBrowserDirectory);
    }
    
    // Initialize VST directory with config value only (no auto-scanning)
    if (!app->uiState.vstDirecory.empty() && 
        std::filesystem::is_directory(app->uiState.vstDirecory)) {
        vstTree.setRootDirectory(app->uiState.vstDirecory);
    }
    // Removed auto-detection to prevent crashes - user must manually select VST directory
    
    buildFileTreeUI();
    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

void FileBrowserComponent::update() {
    
}

bool FileBrowserComponent::handleEvents() {
    if (fileTreeNeedsRebuild || vstTreeNeedsRebuild) {
        buildFileTreeUI();
        fileTreeNeedsRebuild = false;
        vstTreeNeedsRebuild = false;
        forceUpdate = true;
    }

    return forceUpdate;
}

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
    // Removed auto-fallback to prevent crashes - user must manually select directory
}

void FileBrowserComponent::buildFileTreeUI() {
    auto* scrollColumn = static_cast<ScrollableColumn*>(layout);
    if (!scrollColumn) return;

    scrollColumn->clear();

    scrollColumn->addElements({
        spacer(Modifier().setfixedHeight(16).align(Align::TOP)),
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
    
    if (tree.isDirectory()) {
        std::string symbol = tree.isOpen() ? "[-] " : "[+] ";
        displayName = symbol + displayName;
        std::string treePath = tree.getPath();
        
        textModifier.onLClick([this, treePath](){
            toggleTreeNodeByPath(treePath);
            fileTreeNeedsRebuild = true;
        });
    } else if (tree.isAudioFile()) {
        displayName = "[f] " + displayName;
        std::string filePath = tree.getPath();
        
        textModifier.onLClick([this, filePath](){
            juce::File sampleFile(filePath);
            std::string trackName = sampleFile.getFileNameWithoutExtension().toStdString();

            app->addTrack(trackName, filePath);
        });
    } else if (tree.isVSTFile()) {
        displayName = "[v] " + displayName;
        std::string filePath = tree.getPath();
        
        // Add VST to currently selected track - DEFERRED to avoid OpenGL context conflicts
        textModifier.onLClick([this, filePath](){
            std::cout << "FileBrowser: Requesting VST load via unified system: " << filePath << std::endl;
            app->addEffect(filePath);
        });
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
    
    if (tree.isDirectory()) {
        std::string symbol = tree.isOpen() ? "[-] " : "[+] ";
        displayName = symbol + displayName;
        std::string treePath = tree.getPath();
        
        textModifier.onLClick([this, treePath](){
            toggleVSTTreeNodeByPath(treePath);
            vstTreeNeedsRebuild = true;
        });
    } else if (tree.isVSTFile()) {
        displayName = "[v] " + displayName;
        std::string filePath = tree.getPath();
        
        // Add VST to currently selected track - DEFERRED to avoid OpenGL context conflicts
        textModifier.onLClick([this, filePath](){
            std::cout << "FileBrowser: Requesting VST load via unified system: " << filePath << std::endl;
            app->addEffect(filePath);
        });
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