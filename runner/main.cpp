#include <hyprtoolkit-lua/LuaBindings.hpp>

#include <iostream>

using namespace Hyprtoolkit::Lua;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: hyprtoolkit-lua <script.lua>\n";
        return 2;
    }

    const auto state = createLuaState();
    state->openLibs();
    registerAllBindings(state->state());

    if (const auto result = state->doFile(argv[1]); !result) {
        std::cerr << result.error() << '\n';
        return 1;
    }

    return 0;
}
