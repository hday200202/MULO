#include "Application.hpp"
#include "tinyfiledialogs.hpp"

std::string Application::selectDirectory() {
    const char* dir = tinyfd_selectFolderDialog("Select Directory", nullptr);
    return dir ? std::string(dir) : "";
}

std::string Application::selectFile(std::initializer_list<std::string> filters) {
    std::vector<const char*> patterns;
    for (const auto& f : filters) patterns.push_back(f.c_str());
    const char* file = tinyfd_openFileDialog(
        "Select File",
        nullptr,
        patterns.empty() ? 0 : patterns.size(),
        patterns.empty() ? nullptr : patterns.data(),
        nullptr,
        0
    );
    return file ? std::string(file) : "";
}