#pragma once

#include "MULOComponent.hpp"
#include <fstream>

class ExtensionUploader : public MULOComponent {
public:
    ExtensionUploader() { name = "extension_uploader"; }
    ~ExtensionUploader() override {}

    void init() override;
    void update() override;
    bool handleEvents() override;

private:
    sf::RenderWindow window;
    sf::VideoMode resolution;
    sf::View windowView;
    std::unique_ptr<UILO> ui;

    TextBox* extDescriptionTextBox = nullptr;
    TextBox* versionTextBox = nullptr;
    Text* uploadStatusText = nullptr;

    std::string extDescription = "";
    std::string version = "";
    std::vector<std::string> platforms;

    std::string soPath = "";
    std::string dllPath = "";
    std::string dylibPath = "";

    std::string uploadStatus = "";
    bool isProcessingUpload = false;

    Container* initLayout();
    void showWindow();
    void hideWindow();
    void updatePlatformsVector();
    void selectFile(const std::string& platform);
    void uploadExtension();
};

#include "Application.hpp"

void ExtensionUploader::init() {
    if (!app) return;

    app->writeConfig<bool>("extupload_shown", false);

    resolution.size.x = app->getWindow().getSize().x / 3;
    resolution.size.y = app->getWindow().getSize().y / 1.6;
    windowView.setSize(static_cast<sf::Vector2f>(resolution.size));

    initialized = true;
}

void ExtensionUploader::update() {
    static bool prevWindowShown = false;
    bool windowShown = app->readConfig<bool>("extupload_shown", false);

    if (windowShown && !prevWindowShown)
        showWindow();

    if (!windowShown && prevWindowShown)
        hideWindow();

    if (window.isOpen() && ui) {
        uploadStatusText->setString(uploadStatus);
        
        if (ui->getScale() != app->ui->getScale())
            ui->setScale(app->ui->getScale());

        ui->forceUpdate(windowView);

        if (ui->windowShouldUpdate()) {
            window.clear(sf::Color(30, 30, 30));
            ui->render();
            window.display();
        }
    }

    prevWindowShown = windowShown;
}

bool ExtensionUploader::handleEvents() {
    static bool prevShow = false;
    bool showUploader = app->readConfig<bool>("extupload_shown", false);

    if (showUploader && !prevShow)
        showWindow();

    if (!showUploader && prevShow)
        hideWindow();

    prevShow = showUploader;
    return prevShow;
}

Container* ExtensionUploader::initLayout() {
    auto layout = column(
        Modifier().setColor(app->resources.activeTheme->foreground_color),
    contains{
        spacer(Modifier().setfixedHeight(24.f).align(Align::TOP)),

        row(
            Modifier().align(Align::TOP | Align::LEFT).setfixedHeight(96),
        contains{
            spacer(Modifier().setfixedWidth(32).align(Align::TOP | Align::LEFT)),
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(48).setColor(app->resources.activeTheme->primary_text_color),
                "Extension Uploader",
                app->resources.dejavuSansFont,
                "ext_uploader_text"
            ),
        }),

        spacer(Modifier().setfixedHeight(24.f).align(Align::TOP)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Description",
                app->resources.dejavuSansFont,
                "description_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            extDescriptionTextBox = textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill,
                app->resources.dejavuSansFont,
                "Enter Description",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "description_textbox"
            )
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            text(
                Modifier().align(Align::CENTER_Y | Align::LEFT).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                "Version",
                app->resources.dejavuSansFont,
                "version_text"
            ),
        }),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            versionTextBox = textBox(
                Modifier().setfixedHeight(48).setColor(sf::Color::White),
                TBStyle::Pill,
                app->resources.dejavuSansFont,
                "1.0.0",
                app->resources.activeTheme->foreground_color,
                app->resources.activeTheme->button_color,
                "version_textbox"
            )
        }),

        spacer(Modifier().setfixedHeight(16.f)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(120)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::LEFT)
                    .onLClick([&](){ selectFile("linux"); }),
                ButtonStyle::Pill,
                soPath.empty() ? ".so" : "Linux",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(120)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::CENTER_X)
                    .onLClick([&](){ selectFile("windows"); }),
                ButtonStyle::Pill,
                dllPath.empty() ? ".dll" : "Windows",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(120)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::RIGHT)
                    .onLClick([&](){ selectFile("mac"); }),
                ButtonStyle::Pill,
                dylibPath.empty() ? ".dylib" : "macOS",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
        }),

        spacer(Modifier().setfixedHeight(16.f)),

        row(
            Modifier().setfixedHeight(32).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            uploadStatusText = text(
                Modifier().align(Align::CENTER_Y | Align::CENTER_X).setfixedHeight(32).setColor(app->resources.activeTheme->primary_text_color),
                uploadStatus,
                app->resources.dejavuSansFont,
                "upload_status_text"
            ),
        }),

        spacer(Modifier().setfixedHeight(16.f)),

        row(
            Modifier().setfixedHeight(64).setWidth(0.75f).align(Align::CENTER_X | Align::CENTER_Y),
        contains{
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::LEFT)
                    .onLClick([&](){ app->writeConfig<bool>("extupload_shown", false); }),
                ButtonStyle::Pill,
                "Close",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
            spacer(Modifier().setfixedWidth(16)),
            button(
                Modifier()
                    .setfixedHeight(48)
                    .setfixedWidth(96)
                    .setColor(app->resources.activeTheme->button_color)
                    .align(Align::CENTER_Y | Align::RIGHT)
                    .onLClick([&](){ uploadExtension(); }),
                ButtonStyle::Pill,
                "Upload",
                app->resources.dejavuSansFont,
                app->resources.activeTheme->secondary_text_color
            ),
        })
    });

    return layout;
}

void ExtensionUploader::showWindow() {
    if (window.isOpen()) return;

    auto mainPos = app->getWindow().getPosition();
    auto mainSize = app->getWindow().getSize();
    int centerX = mainPos.x + (int(mainSize.x) - int(resolution.size.x)) / 2;
    int centerY = mainPos.y + (int(mainSize.y) - int(resolution.size.y)) / 2;

    sf::ContextSettings settings;
    settings.antiAliasingLevel = 8;
    window.create(resolution, "Extension Uploader", sf::Style::None, sf::State::Windowed, settings);
    window.setPosition(sf::Vector2i(centerX, centerY));
    window.requestFocus();

    app->ui->setInputBlocked(true);

    ui = std::make_unique<UILO>(window, windowView);
    ui->addPage(page({initLayout()}), "upload_page");
    ui->switchToPage("upload_page");
    ui->forceUpdate();
}

void ExtensionUploader::hideWindow() {
    if (!window.isOpen()) return;

    ui.reset();
    window.close();
    cleanupMarkedElements();
    app->ui->setInputBlocked(false);
}

void ExtensionUploader::updatePlatformsVector() {
    platforms.clear();
    if (!soPath.empty()) platforms.push_back("linux");
    if (!dllPath.empty()) platforms.push_back("windows");
    if (!dylibPath.empty()) platforms.push_back("mac");
}

void ExtensionUploader::selectFile(const std::string& platform) {
    std::string filter;
    std::string* pathPtr = nullptr;
    
    if (platform == "linux") {
        filter = "*.so";
        pathPtr = &soPath;
    } else if (platform == "windows") {
        filter = "*.dll";
        pathPtr = &dllPath;
    } else if (platform == "mac") {
        filter = "*.dylib";
        pathPtr = &dylibPath;
    }
    
    if (pathPtr) {
        std::string selectedFile = app->selectFile({filter});
        
        if (!selectedFile.empty()) {
            *pathPtr = selectedFile;
            updatePlatformsVector();
            
            size_t lastSlash = selectedFile.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ? 
                selectedFile.substr(lastSlash + 1) : selectedFile;
            uploadStatus = "Selected: " + filename;
        }
    }
}

void ExtensionUploader::uploadExtension() {
    if (isProcessingUpload) return;
    
    if (!app->isUserLoggedIn()) {
        uploadStatus = "Must Be Logged In";
        return;
    }
    
    if (extDescriptionTextBox && versionTextBox) {
        extDescription = extDescriptionTextBox->getText();
        version = versionTextBox->getText();
    }
    
    if (extDescription.empty()) {
        uploadStatus = "Must Enter Description";
        return;
    }
    
    if (version.empty()) {
        uploadStatus = "Must Enter Version";
        return;
    }
    
    updatePlatformsVector();
    if (platforms.empty()) {
        uploadStatus = "Must Select At Least One File";
        return;
    }
    
    isProcessingUpload = true;
    uploadStatus = "Uploading";
    
    std::string authorName = "Unknown";
    std::string email = app->getCurrentUserEmail();
    if (!email.empty() && email.find("@MULO.local") != std::string::npos) {
        authorName = email.substr(0, email.find("@MULO.local"));
    } else if (!email.empty()) {
        authorName = email;
    }
    
    std::string extensionName = "Unknown";
    std::string firstFilePath = !soPath.empty() ? soPath : 
                               !dllPath.empty() ? dllPath : dylibPath;
    
    if (!firstFilePath.empty()) {
        size_t lastSlash = firstFilePath.find_last_of("/\\");
        std::string filename = (lastSlash != std::string::npos) ? 
            firstFilePath.substr(lastSlash + 1) : firstFilePath;
        
        size_t lastDot = filename.find_last_of('.');
        if (lastDot != std::string::npos) {
            extensionName = filename.substr(0, lastDot);
        }
    }
    
    std::string platformString = "";
    for (size_t i = 0; i < platforms.size(); ++i) {
        if (i > 0) platformString += "/";
        if (platforms[i] == "linux") platformString += "linux";
        else if (platforms[i] == "windows") platformString += "win";
        else if (platforms[i] == "mac") platformString += "mac";
    }
    
    // TODO: Implement actual Firebase upload with structure:
    // {
    //   "author": authorName,
    //   "description": extDescription, 
    //   "name": extensionName,
    //   "platform": platformString,
    //   "verified": false,
    //   "version": version,
    //   "download_url": "generated_after_upload"
    // }
    
    Application::ExtensionData extData;
    extData.author = authorName;
    extData.description = extDescription;
    extData.name = extensionName;
    extData.version = version;
    extData.verified = false;
    
    std::vector<std::string> filePaths;
    if (!soPath.empty()) filePaths.push_back(soPath);
    if (!dllPath.empty()) filePaths.push_back(dllPath);
    if (!dylibPath.empty()) filePaths.push_back(dylibPath);
    
    if (filePaths.empty()) {
        uploadStatus = "No files selected";
        isProcessingUpload = false;
        return;
    }
    
    // Verify files exist before uploading
    for (const auto& path : filePaths) {
        std::ifstream test(path);
        if (!test.is_open()) {
            uploadStatus = "Cannot access file: " + path;
            isProcessingUpload = false;
            return;
        }
        test.close();
    }
    
    app->uploadExtension(extData, filePaths, [this](Application::FirebaseState state, const std::string& message) {
        isProcessingUpload = false;
        if (state == Application::FirebaseState::Success) {
            uploadStatus = "Uploaded";
        } else {
            uploadStatus = "Upload Failed: " + message;
        }
    });
    
    // Don't set isProcessingUpload = false here since the callback will handle it
}

GET_INTERFACE
DECLARE_PLUGIN(ExtensionUploader)