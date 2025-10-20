#pragma once

#ifdef _WIN32
    #include <windows.h>
    #define PLUGIN_EXT ".dll"
#elif __APPLE__
    #include <dlfcn.h>
    #define PLUGIN_EXT ".dylib"
#else
    #include <dlfcn.h>
    #define PLUGIN_EXT ".so"
#endif

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef Success
#undef Success
#endif
#ifdef None
#undef None
#endif
#ifdef True
#undef True
#endif
#ifdef False
#undef False
#endif
#endif

#ifdef _WIN32
#include <windows.h>

static POINT s_minWindowSize = {800, 600};

LRESULT CALLBACK MinSizeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_GETMINMAXINFO) {
        MINMAXINFO* mmi = (MINMAXINFO*)lParam;
        mmi->ptMinTrackSize.x = s_minWindowSize.x;
        mmi->ptMinTrackSize.y = s_minWindowSize.y;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void SetMinWindowSize(HWND hwnd, int minWidth, int minHeight)
{
    s_minWindowSize.x = minWidth;
    s_minWindowSize.y = minHeight;
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)MinSizeWndProc);
}
#endif

#ifdef __APPLE__
#include <objc/objc.h>
#include <objc/message.h>
#include <CoreGraphics/CoreGraphics.h>
#endif

#ifdef FIREBASE_AVAILABLE
    #include <firebase/app.h>
    #include <firebase/firestore.h>
    #include <firebase/database.h>
    #include <firebase/auth.h>
    #include <firebase/storage.h>
#endif
