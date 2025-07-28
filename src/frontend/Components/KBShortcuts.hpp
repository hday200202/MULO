#pragma once

#include "MULOComponent.hpp"

class KBShortcuts : public MULOComponent {
public:
    inline KBShortcuts() { name = "keyboard_shortcuts"; }
    inline ~KBShortcuts() override {}

    inline void init() override { initialized = true; }
    inline void update() override {}
    inline Container* getLayout() { return layout; }

    bool handleEvents();
};