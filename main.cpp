#include "engine.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    bool headless = false;
    bool cli = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--headless") {
            headless = true;
        } else if (arg == "--cli") {
            cli = true;
        }
    }

    if (headless) {
        Engine engine;
        engine.runHeadless(500, 1337);
        return 0;
    }

    if (cli) {
        Engine engine;
        engine.runInteractive();
        return 0;
    }

    // Default: launch native 2D Win32 GUI window
    Engine engine;
    engine.runGui();
    return 0;
}
