
#include "FileTree.hpp"
#include "../audio/VSTPluginManager.hpp"
#include <algorithm>
#include <iostream>

FileTree::FileTree(const std::string& rootDirectoryPath) {
    setRootDirectory(rootDirectoryPath);
}

bool FileTree::isOpen() const {
    return open;
}

void FileTree::setOpen(bool openState) {
    open = openState;
    if (open && !childrenLoaded) {
        loadChildren();
    }
}

void FileTree::toggleOpen() {
    setOpen(!open);
}

void FileTree::setRootDirectory(const std::string& directoryPath) {
    if (std::filesystem::exists(directoryPath) && std::filesystem::is_directory(directoryPath)) {
        path = std::filesystem::absolute(directoryPath).string();
        name = std::filesystem::path(path).filename().string();
        if (name.empty()) {
            name = path; // Root directory case
        }
        isDir = true;
        childrenLoaded = false;
        subDirectories.clear();
        files.clear();
    }
}

void FileTree::refresh() {
    if (isDir) {
        childrenLoaded = false;
        subDirectories.clear();
        files.clear();
        if (open) {
            loadChildren();
        }
    }
}

void FileTree::loadChildren() {
    if (!isDir || childrenLoaded) return;
    
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            auto child = std::make_shared<FileTree>();
            child->path = entry.path().string();
            child->name = entry.path().filename().string();
            child->parent = this; // Set raw pointer to parent
            
            if (entry.is_directory()) {
                // Check if this is a VST bundle using cross-platform manager
                auto& vstManager = VSTPluginManager::getInstance();
                if (vstManager.isValidVSTFile(child->path)) {
                    child->isDir = false; // Treat VST bundles as files
                    files.push_back(child);
                } else {
                    child->isDir = true;
                    subDirectories.push_back(child);
                }
            } else if (entry.is_regular_file()) {
                child->isDir = false;
                files.push_back(child);
            }
        }
        
        // Sort directories and files alphabetically
        std::sort(subDirectories.begin(), subDirectories.end(),
                  [](const auto& a, const auto& b) {
                      return a->getName() < b->getName();
                  });
        
        std::sort(files.begin(), files.end(),
                  [](const auto& a, const auto& b) {
                      return a->getName() < b->getName();
                  });
        
        childrenLoaded = true;
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error loading directory: " << e.what() << std::endl;
    }
}

const std::string& FileTree::getPath() const {
    return path;
}

const std::string& FileTree::getName() const {
    return name;
}

bool FileTree::isDirectory() const {
    return isDir;
}

bool FileTree::isAudioFile() const {
    if (isDir) return false;
    
    std::string ext = getFileExtension(name);
    return isValidAudioExtension(ext);
}

bool FileTree::isVSTFile() const {
    if (isDir) return false;
    
    std::string ext = getFileExtension(name);
    return isValidVSTExtension(ext);
}

const std::vector<std::shared_ptr<FileTree>>& FileTree::getSubDirectories() const {
    return subDirectories;
}

const std::vector<std::shared_ptr<FileTree>>& FileTree::getFiles() const {
    return files;
}

FileTree* FileTree::getParent() const {
    return parent;
}

bool FileTree::isValidAudioExtension(const std::string& extension) {
    static const std::vector<std::string> audioExtensions = {
        ".wav", ".mp3", ".flac", ".aiff", ".ogg", ".m4a", ".wma"
    };
    
    std::string lowerExt = extension;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
    
    return std::find(audioExtensions.begin(), audioExtensions.end(), lowerExt) != audioExtensions.end();
}

bool FileTree::isValidVSTExtension(const std::string& extension) {
    auto& vstManager = VSTPluginManager::getInstance();
    auto validExtensions = vstManager.getVSTExtensions();
    
    std::string lowerExt = extension;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
    
    for (auto& ext : validExtensions) {
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == lowerExt) {
            return true;
        }
    }
    
    return false;
}

std::string FileTree::getFileExtension(const std::string& filename) const {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos != std::string::npos && dotPos < filename.length() - 1) {
        return filename.substr(dotPos);
    }
    return "";
}

void FileTree::loadFromPath(const std::string& directoryPath) {
    setRootDirectory(directoryPath);
}