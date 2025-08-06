#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
#elif defined(__APPLE__)
    #include <CoreFoundation/CoreFoundation.h>
#else
    #include <unistd.h>
    #include <pwd.h>
#endif

class VSTPluginManager {
public:
    struct VSTInfo {
        std::string path;
        std::string name;
        std::string category;
        bool isValid = false;
    };

    static VSTPluginManager& getInstance() {
        static VSTPluginManager instance;
        return instance;
    }

    // Get platform-specific default VST3 search paths
    std::vector<std::string> getDefaultVSTSearchPaths() const;
    
    // Check if a file is a valid VST plugin for the current platform
    bool isValidVSTFile(const std::string& filepath) const;
    
    // Scan a directory for VST plugins
    std::vector<VSTInfo> scanDirectory(const std::string& directory, bool recursive = true) const;
    
    // Get file extension for VST plugins on current platform
    std::vector<std::string> getVSTExtensions() const;

private:
    VSTPluginManager() = default;
    ~VSTPluginManager() = default;
    VSTPluginManager(const VSTPluginManager&) = delete;
    VSTPluginManager& operator=(const VSTPluginManager&) = delete;
};

// Inline implementations to make this header-only
inline std::vector<std::string> VSTPluginManager::getDefaultVSTSearchPaths() const {
    std::vector<std::string> paths;
    
#ifdef _WIN32
    // Windows VST3 paths
    paths.push_back("C:\\Program Files\\Common Files\\VST3");
    paths.push_back("C:\\Program Files (x86)\\Common Files\\VST3");
    
    // User-specific paths
    char* appDataPath = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appDataPath, &len, "APPDATA") == 0 && appDataPath != nullptr) {
        paths.push_back(std::string(appDataPath) + "\\VST3");
        free(appDataPath);
    }
    
    // Legacy VST2 paths (some VST3 plugins might be here)
    paths.push_back("C:\\Program Files\\VSTPlugins");
    paths.push_back("C:\\Program Files (x86)\\VSTPlugins");
    paths.push_back("C:\\Program Files\\Steinberg\\VSTPlugins");
    paths.push_back("C:\\Program Files (x86)\\Steinberg\\VSTPlugins");
    
#elif defined(__APPLE__)
    // macOS VST3 paths
    paths.push_back("/Library/Audio/Plug-Ins/VST3");
    paths.push_back("/System/Library/Audio/Plug-Ins/VST3");
    
    // User-specific paths
    char* homeDir = getenv("HOME");
    if (homeDir) {
        paths.push_back(std::string(homeDir) + "/Library/Audio/Plug-Ins/VST3");
    }
    
    // Legacy VST paths
    paths.push_back("/Library/Audio/Plug-Ins/VST");
    if (homeDir) {
        paths.push_back(std::string(homeDir) + "/Library/Audio/Plug-Ins/VST");
    }
    
#else
    // Linux VST3 paths
    paths.push_back("/usr/lib/vst3");
    paths.push_back("/usr/local/lib/vst3");
    
    // User-specific paths
    char* homeDir = getenv("HOME");
    if (homeDir) {
        paths.push_back(std::string(homeDir) + "/.vst3");
        paths.push_back(std::string(homeDir) + "/.local/lib/vst3");
    }
    
    // Legacy VST paths
    paths.push_back("/usr/lib/vst");
    paths.push_back("/usr/local/lib/vst");
    if (homeDir) {
        paths.push_back(std::string(homeDir) + "/.vst");
        paths.push_back(std::string(homeDir) + "/.local/lib/vst");
    }
    
    // Additional common Linux paths
    paths.push_back("/usr/lib/lxvst");
    paths.push_back("/usr/local/lib/lxvst");
    if (homeDir) {
        paths.push_back(std::string(homeDir) + "/.lxvst");
    }
#endif

    // Filter out non-existent paths
    std::vector<std::string> existingPaths;
    for (const auto& path : paths) {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            existingPaths.push_back(path);
        }
    }
    
    return existingPaths;
}

inline std::vector<std::string> VSTPluginManager::getVSTExtensions() const {
    std::vector<std::string> extensions;
    
    // VST3 is supported on all platforms
    extensions.push_back(".vst3");
    
#ifdef _WIN32
    // Windows: VST plugins can be .dll files
    extensions.push_back(".dll");
#elif defined(__APPLE__)
    // macOS: VST plugins can be .dylib files or .vst bundles
    extensions.push_back(".dylib");
    extensions.push_back(".vst");
#else
    // Linux: VST plugins can be .so files
    extensions.push_back(".so");
#endif

    return extensions;
}

inline bool VSTPluginManager::isValidVSTFile(const std::string& filepath) const {
    if (!std::filesystem::exists(filepath)) {
        return false;
    }
    
    std::filesystem::path path(filepath);
    std::string extension = path.extension().string();
    
    // Convert to lowercase for case-insensitive comparison
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    auto validExtensions = getVSTExtensions();
    for (auto& ext : validExtensions) {
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    
    bool hasValidExtension = std::find(validExtensions.begin(), validExtensions.end(), extension) != validExtensions.end();
    
    if (!hasValidExtension) {
        return false;
    }
    
    // Special handling for VST3 bundles on macOS and directories on other platforms
    if (extension == ".vst3") {
        // VST3 can be either a file or a bundle/directory
        return std::filesystem::exists(filepath);
    }
    
#ifdef __APPLE__
    // macOS .vst bundles are directories
    if (extension == ".vst") {
        return std::filesystem::is_directory(filepath);
    }
#endif
    
    // For other extensions, ensure it's a regular file
    return std::filesystem::is_regular_file(filepath);
}

inline std::vector<VSTPluginManager::VSTInfo> VSTPluginManager::scanDirectory(const std::string& directory, bool recursive) const {
    std::vector<VSTInfo> plugins;
    
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        return plugins;
    }
    
    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file() || entry.is_directory()) {
                    std::string filepath = entry.path().string();
                    
                    if (isValidVSTFile(filepath)) {
                        VSTInfo info;
                        info.path = filepath;
                        info.name = entry.path().stem().string();
                        plugins.push_back(info);
                    }
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file() || entry.is_directory()) {
                    std::string filepath = entry.path().string();
                    
                    if (isValidVSTFile(filepath)) {
                        VSTInfo info;
                        info.path = filepath;
                        info.name = entry.path().stem().string();
                        info.category = "Plugin"; // Could be enhanced to detect actual category
                        info.isValid = true;
                        
                        plugins.push_back(info);
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& ex) {
        std::cerr << "Error scanning VST directory " << directory << ": " << ex.what() << std::endl;
    }
    
    return plugins;
}
