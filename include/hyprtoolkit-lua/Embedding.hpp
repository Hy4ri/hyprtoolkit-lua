#pragma once

#include <expected>
#include <functional>
#include <string>
#include <string_view>

struct lua_State;

namespace Hyprtoolkit::Lua {
    struct SCallbackHooks {
        // Same stack/status contract as lua_pcall(state, arguments, results, 0).
        // Hooks must not yield, close the state, or longjmp across C++ frames.
        std::function<int(lua_State*, int arguments, int results)> protectedCall;
        std::function<void(std::string_view)>                      reportError;
    };

    // Configure registered bindings on their owner thread, outside callbacks.
    // Empty hooks restore lua_pcall/stderr defaults. The host executes scripts.
    std::expected<void, std::string> setCallbackHooks(lua_State* state, SCallbackHooks hooks);

    // Permanently retire registered bindings before lua_close(). Call on the
    // owner thread, outside callbacks. Does not destroy the backend or elements.
    std::expected<void, std::string> retireState(lua_State* state);
}
