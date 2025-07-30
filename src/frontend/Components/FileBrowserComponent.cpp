#include "FileBrowserComponent.hpp"
#include "Application.hpp"
#include "Engine.hpp"
#include <juce_core/juce_core.h>

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
            .setColor(resources->activeTheme->track_color),
        contains{},
        "file_browser_scroll_column"
    );
    buildFileTreeUI();
    if (parentContainer) {
        parentContainer->addElement(layout);
        initialized = true;
    }
}

void FileBrowserComponent::update() {
    
}

bool FileBrowserComponent::handleEvents() {
    if (fileTreeNeedsRebuild) {
        buildFileTreeUI();
        fileTreeNeedsRebuild = false;
        forceUpdate = true;
    }

    return forceUpdate;
}

void FileBrowserComponent::browseForDirectory() {
    std::string selectedDir = app->selectDirectory();
    if (!selectedDir.empty() && std::filesystem::is_directory(selectedDir)) {
        fileTree.setRootDirectory(selectedDir);
        fileTreeNeedsRebuild = true;
    }
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
                Modifier().align(Align::LEFT | Align::CENTER_Y).setfixedHeight(32).setColor(resources->activeTheme->primary_text_color),
                "user library",
                resources->dejavuSansFont
            ),

            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(resources->activeTheme->alt_button_color)
                    .align(Align::RIGHT | Align::CENTER_Y)
                    .onLClick([this](){ browseForDirectory(); }),
                ButtonStyle::Pill,
                "browse",
                resources->dejavuSansFont,
                resources->activeTheme->secondary_text_color,
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
                .setColor(resources->activeTheme->primary_text_color)
                .onLClick([this](){
                    fileTree.toggleOpen();
                    fileTreeNeedsRebuild = true;
            }),
            displayName,
            resources->dejavuSansFont
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
}

void FileBrowserComponent::buildFileTreeUIRecursive(const FileTree& tree, int indentLevel) {
    auto* scrollColumn = static_cast<ScrollableColumn*>(layout);
    if (!scrollColumn) return;

    float indent = indentLevel * 20.f;
    std::string displayName = tree.getName();
    Modifier textModifier = Modifier().setfixedHeight(28).setColor(resources->activeTheme->primary_text_color);
    
    if (tree.isDirectory()) {
        std::string symbol = tree.isOpen() ? "[-] " : "[+] ";
        displayName = symbol + displayName;
        std::string treePath = tree.getPath();
        
        // FIX: Lambda now only sets the rebuild flag.
        textModifier.onLClick([this, treePath](){
            toggleTreeNodeByPath(treePath);
            fileTreeNeedsRebuild = true;
        });
    } else if (tree.isAudioFile()) {
        displayName = "[f] " + displayName;
        std::string filePath = tree.getPath();
        
        // FIX: Lambda now only adds the track. The TimelineComponent will handle the UI.
        textModifier.onLClick([this, filePath](){
            if (!engine) return;

            juce::File sampleFile(filePath);
            std::string trackName = sampleFile.getFileNameWithoutExtension().toStdString();

            engine->addTrack(trackName, filePath);
        });
    }

    auto textElement = text(textModifier, displayName, resources->dejavuSansFont);

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