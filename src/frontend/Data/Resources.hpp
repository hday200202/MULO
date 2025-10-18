#pragma once

#include <string>
#include <initializer_list>

namespace sf { class WindowBase; }
struct UIResources;

namespace Resources {
    void initUIResources(UIResources& resources, const std::string& exeDirectory);
}
