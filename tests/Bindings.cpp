#include "Binding.hpp"

#include <hyprtoolkit-lua/Embedding.hpp>
#include <hyprtoolkit-lua/LuaBindings.hpp>
#include <hyprutils/signal/Signal.hpp>

#include <iostream>

using namespace Hyprtoolkit::Lua;

static void check(bool condition, std::string_view message) {
    if (!condition)
        throw std::runtime_error(std::string{message});
}

static void run(lua_State* state, const char* code) {
    if (luaL_dostring(state, code) == LUA_OK)
        return;
    const std::string message = lua_tostring(state, -1);
    lua_pop(state, 1);
    throw std::runtime_error(message);
}

static LuaCallback readCallback(lua_State* state, const char* code) {
    run(state, code);
    auto result = callback(state, -1);
    lua_pop(state, 1);
    return result;
}

static void retirement() {
    LuaCallback retained;
    {
        CLuaState lua;
        lua.openLibs();
        registerAllBindings(lua.state());
        retained = readCallback(lua.state(), R"(
            weak = setmetatable({}, { __mode = "v" })
            local captured = {}
            weak[1] = captured
            return function() return captured end
        )");
        run(lua.state(), "collectgarbage('collect'); assert(weak[1])");
        check(retained->beginCall(), "live callback must execute");
        check(retained->finishCall(0, 1), "callback result");
        lua_pop(lua.state(), 1);

        check(retireState(lua.state()).has_value(), "retire state");
        check(retireState(lua.state()).has_value(), "retirement is idempotent");
        check(!retained->beginCall(), "retired callback must not execute");
        run(lua.state(), R"(
            collectgarbage('collect')
            assert(weak[1] == nil)
            assert(not pcall(function() hyprtoolkit.Backend.create() end))
        )");
        check(lua_gettop(lua.state()) == 0, "retirement preserves the stack");
    }
    check(!retained->beginCall(), "callback must remain inert after lua_close");
}

static void signalRetirement() {
    CLuaState lua;
    lua.openLibs();
    registerAllBindings(lua.state());
    Hyprutils::Signal::CSignalT<> signal;
    const auto                    cb = readCallback(lua.state(), R"(
        calls = 0
        weak = setmetatable({}, { __mode = "v" })
        local connection
        function setConnection(value) connection = value; weak[1] = value end
        return function() calls = calls + 1; return connection end
    )");
    lua_getglobal(lua.state(), "setConnection");
    pushConnection(lua.state(), signal.listen([cb] {
        if (cb->beginCall())
            cb->finishCall(0);
    }));
    check(lua_pcall(lua.state(), 1, 0, 0) == LUA_OK, "capture connection");
    run(lua.state(), "setConnection = nil; collectgarbage('collect'); assert(weak[1]:connected())");
    signal.emit();
    run(lua.state(), "assert(calls == 1)");
    check(retireState(lua.state()).has_value(), "retire signal connection");
    signal.emit();
    run(lua.state(), "collectgarbage('collect'); assert(weak[1] == nil); assert(calls == 1)");
}

static void retirementDuringCallback() {
    CLuaState lua;
    lua.openLibs();
    registerAllBindings(lua.state());
    lua_pushcfunction(lua.state(), [](lua_State* state) {
        lua_pushboolean(state, !retireState(state).has_value());
        return 1;
    });
    lua_setglobal(lua.state(), "tryRetire");
    const auto cb = readCallback(lua.state(), "return function() assert(tryRetire()) end");
    check(cb->beginCall() && cb->finishCall(0), "reentrant retirement is rejected");
    check(cb->beginCall() && cb->finishCall(0), "rejected retirement leaves context active");
    check(retireState(lua.state()).has_value(), "quiescent retirement succeeds");
}

static void rawStateClose() {
    auto* state = luaL_newstate();
    check(state != nullptr, "create external Lua state");
    registerAllBindings(state);
    const auto cb = readCallback(state, "return function() end");
    lua_close(state);
    check(!cb->beginCall(), "GC fallback retires externally owned states");
}

int main() {
    try {
        retirement();
        signalRetirement();
        retirementDuringCallback();
        rawStateClose();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
