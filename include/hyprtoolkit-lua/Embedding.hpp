#pragma once

#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <hyprutils/memory/SharedPtr.hpp>

struct lua_State;

namespace Hyprtoolkit {
    class IBackend;
    class IElement;
}

namespace Hyprtoolkit::Lua {
    // Attach an existing backend to registered bindings. The host owns all
    // backend/surface lifecycle and must retire bindings before backend shutdown.
    // One attachment per Lua state; Lua backend/window creation is disabled.
    std::expected<void, std::string> attachBackend(lua_State* state, const Hyprutils::Memory::CSharedPointer<IBackend>& backend);

    // Push one element reference (or nil), without transferring backend ownership.
    // Requires registered, live bindings on the owner thread. Invalid use throws
    // a C++ exception; Lua allocations require the usual protected Lua boundary.
    void pushElement(lua_State* state, Hyprutils::Memory::CSharedPointer<IElement> element);

    // Leaves the stack unchanged. Nil is an empty pointer; invalid/finalized
    // userdata or inactive bindings return an error without raising a Lua error.
    std::expected<Hyprutils::Memory::CSharedPointer<IElement>, std::string> getElement(lua_State* state, int index);

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
