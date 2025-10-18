#pragma once

#include <SFML/Window.hpp>
#include <string>

namespace PlatformUtils {
void setMinimumWindowSize(sf::WindowBase& window, int minWidth, int minHeight);
std::string getExecutableDirectory();
}
