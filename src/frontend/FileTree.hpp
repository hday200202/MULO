
#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include <functional>

class FileTree {
public:
    FileTree() = default;
    
    FileTree(const std::string& rootDirectoryPath);

    // Tree state
    bool isOpen() const;
    
    void setOpen(bool open);
    
    void toggleOpen();

    // Directory operations
    void setRootDirectory(const std::string& path);
    
    void refresh();
    
    void loadChildren();

    // Getters
    const std::string& getPath() const;
    
    const std::string& getName() const;
    
    bool isDirectory() const;
    
    bool isAudioFile() const;
    
    bool isVSTFile() const;
    
    // Tree navigation
    const std::vector<std::shared_ptr<FileTree>>& getSubDirectories() const;
    
    const std::vector<std::shared_ptr<FileTree>>& getFiles() const;
    
    FileTree* getParent() const;
    
    // File filtering
    static bool isValidAudioExtension(const std::string& extension);
    
    static bool isValidVSTExtension(const std::string& extension);

private:
    std::string path;
    std::string name;
    FileTree* parent = nullptr;
    
    std::vector<std::shared_ptr<FileTree>> subDirectories;
    std::vector<std::shared_ptr<FileTree>> files;
    
    bool open = false;
    bool isDir = false;
    bool childrenLoaded = false;
    
    // Helper methods
    void loadFromPath(const std::string& directoryPath);
    
    std::string getFileExtension(const std::string& filename) const;
};