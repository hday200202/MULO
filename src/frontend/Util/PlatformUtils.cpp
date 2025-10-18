#include "Application.hpp"
#include "PlatformDefines.hpp"

namespace PlatformUtils {

#ifdef __linux__
void setMinimumWindowSize(sf::WindowBase& window, int minWidth, int minHeight) {
    Display* display = XOpenDisplay(nullptr);
    if (display) {
        XSizeHints hints;
        hints.flags = PMinSize;
        hints.min_width = minWidth;
        hints.min_height = minHeight;
        Window win = static_cast<Window>(window.getNativeHandle());
        XSetWMNormalHints(display, win, &hints);
        XCloseDisplay(display);
    }
}
#elif _WIN32
void setMinimumWindowSize(sf::WindowBase& window, int minWidth, int minHeight) {
    static POINT s_minWindowSize = {minWidth, minHeight};
    HWND hwnd = (HWND)window.getNativeHandle();
    SetMinWindowSize(hwnd, minWidth, minHeight);
}
#elif __APPLE__
void setMinimumWindowSize(sf::WindowBase& window, int minWidth, int minHeight) {
    void* nsWindow = (void*)window.getNativeHandle();
    typedef void* (*GetContentViewFunc)(void*, SEL);
    SEL getContentView = sel_registerName("contentView");
    GetContentViewFunc getView = (GetContentViewFunc)objc_msgSend;
    void* nsView = getView(nsWindow, getContentView);

    typedef void (*SetMinSizeFunc)(void*, SEL, CGSize);
    SetMinSizeFunc setMinSize = (SetMinSizeFunc)objc_msgSend;
    SEL sel = sel_registerName("setContentMinSize:");
    CGSize size = CGSizeMake(minWidth, minHeight);
    setMinSize(nsWindow, sel, size);
}
#else
void setMinimumWindowSize(sf::WindowBase& window, int minWidth, int minHeight) {
    
}
#endif

std::string getExecutableDirectory() {
#ifdef __linux__
    char buffer[1024];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        std::string path(buffer);
        return path.substr(0, path.find_last_of('/'));
    }
    return ".";
#elif _WIN32
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    std::string path(buffer);
    return path.substr(0, path.find_last_of('\\'));
#else
    return ".";
#endif
}

}
