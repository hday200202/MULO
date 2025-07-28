#pragma once

#include "MULOComponent.hpp"
#include "FileTree.hpp"

class FileBrowserComponent : public MULOComponent {
public:
    FileBrowserComponent();
    ~FileBrowserComponent() override;

    void init() override;
    void update() override;
    bool handleEvents() override;

private:
    FileTree fileTree;
    bool fileTreeNeedsRebuild = false;

    void buildFileTreeUI();
    void buildFileTreeUIRecursive(const FileTree& tree, int indentLevel);

    void toggleTreeNodeByPath(const std::string& path);
    void browseForDirectory();
};