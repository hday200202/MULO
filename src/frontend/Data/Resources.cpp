#include "Resources.hpp"
#include "UIData.hpp"
#include <tinyfiledialogs/tinyfiledialogs.hpp>
#include <SFML/Graphics.hpp>
#include <filesystem>

namespace fs = std::filesystem;

void Resources::initUIResources(UIResources& resources, const std::string& exeDirectory) {
    auto findFont = [&exeDirectory](const std::string& filename) -> std::string {
        fs::path cwdFont = fs::current_path() / "assets" / "fonts" / filename;
        if (fs::exists(cwdFont)) return cwdFont.string();

        fs::path exeFont = fs::path(exeDirectory) / "assets" / "fonts" / filename;
        if (fs::exists(exeFont)) return exeFont.string();

        return "";
    };

    auto findIcon = [&exeDirectory](const std::string& filename) -> std::string {
        fs::path cwdIcon = fs::current_path() / "assets" / "icons" / filename;
        if (fs::exists(cwdIcon)) return cwdIcon.string();

        fs::path exeIcon = fs::path(exeDirectory) / "assets" / "icons" / filename;
        if (fs::exists(exeIcon)) return exeIcon.string();

        return "";
    };

    // Load fonts
    resources.dejavuSansFont     = findFont("DejaVuSans.ttf");
    resources.spaceMonoFont      = findFont("SpaceMono-Regular.ttf");
    resources.ubuntuBoldFont     = findFont("ubuntu.bold.ttf");
    resources.ubuntuMonoFont     = findFont("ubuntu.mono.ttf");
    resources.ubuntuMonoBoldFont = findFont("ubuntu.mono-bold.ttf");

    // Load icons
    resources.playIcon          = sf::Image(findIcon("play.png"));
    resources.pauseIcon         = sf::Image(findIcon("pause.png"));
    resources.settingsIcon      = sf::Image(findIcon("settings.png"));
    resources.pianoRollIcon     = sf::Image(findIcon("piano.png"));
    resources.loadIcon          = sf::Image(findIcon("load.png"));
    resources.saveIcon          = sf::Image(findIcon("save.png"));
    resources.exportIcon        = sf::Image(findIcon("export.png"));
    resources.folderIcon        = sf::Image(findIcon("folder.png"));
    resources.openFolderIcon    = sf::Image(findIcon("openfolder.png"));
    resources.pluginFileIcon    = sf::Image(findIcon("pluginfile.png"));
    resources.audioFileIcon     = sf::Image(findIcon("audiofile.png"));
    resources.metronomeIcon     = sf::Image(findIcon("metronome.png"));
    resources.mixerIcon         = sf::Image(findIcon("mixer.png"));
    resources.storeIcon         = sf::Image(findIcon("store.png"));
    resources.fileIcon          = sf::Image(findIcon("file.png"));
    resources.automationIcon    = sf::Image(findIcon("showautomation.png"));
    resources.collabIcon        = sf::Image(findIcon("collab.png"));
    resources.loginIcon         = sf::Image(findIcon("login.png"));
}
