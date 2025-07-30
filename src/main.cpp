#include "frontend/Application.hpp"

int main() {
    Application app;

    while (app.isRunning()) {
        app.update();
        app.render();
    }

    return 0;
}