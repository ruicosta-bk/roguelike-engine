#include "engine.hpp"

#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--headless") {
            headless = true;
            break;
        }
    }

    if (headless) {
        Engine engine;
        engine.runHeadless(500, 1337);
        return 0;
    }

    Engine engine;
    engine.runInteractive();
    return 0;
}
