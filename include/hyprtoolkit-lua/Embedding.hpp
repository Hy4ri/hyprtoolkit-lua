#pragma once

#include <expected>
#include <string>

struct lua_State;

namespace Hyprtoolkit::Lua {
    // Permanently retire registered bindings before lua_close(). Call on the
    // owner thread, outside callbacks. Does not destroy the backend or elements.
    std::expected<void, std::string> retireState(lua_State* state);
}
