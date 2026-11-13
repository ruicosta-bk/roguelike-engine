#include "engine.hpp"

#include <iostream>
#include <string>
#include <fstream>

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
        std::ifstream f("savegame.dat");
        if (f.good()) {
            f.close();
            engine.loadGame("savegame.dat");
        }
        engine.runInteractive();
        return 0;
    }

    // Default: launch native 2D Win32 GUI window
    Engine engine;
    std::ifstream f("savegame.dat");
    if (f.good()) {
        f.close();
        engine.loadGame("savegame.dat");
    }
    engine.runGui();
    return 0;
}
