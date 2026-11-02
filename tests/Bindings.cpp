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

static void rawStateCloseOnAnotherThread() {
    auto* state = luaL_newstate();
    check(state != nullptr, "create external Lua state");
    registerAllBindings(state);
    const auto  cb                 = readCallback(state, "return function() end");
    bool        retirementRejected = false;
    std::thread closer([state, &retirementRejected] {
        retirementRejected = !retireState(state).has_value();
        lua_close(state);
    });
    closer.join();
    check(retirementRejected, "explicit retirement must use the owner thread");
    check(!cb->beginCall() && cb->state() == nullptr, "raw close invalidates retained callbacks even off-thread");
}

static void callbackHooks() {
    CLuaState lua;
    lua.openLibs();
    registerAllBindings(lua.state());
    int                      calls = 0;
    std::vector<std::string> errors;
    check(setCallbackHooks(lua.state(),
                           {
                               .protectedCall =
                                   [&calls](lua_State* state, int arguments, int results) {
                                       ++calls;
                                       return lua_pcall(state, arguments, results, 0);
                                   },
                               .reportError =
                                   [&errors, &lua](std::string_view error) {
                                       check(!retireState(lua.state()), "retirement from error hooks must be rejected");
                                       check(!setCallbackHooks(lua.state(), {}), "changing executing hooks must be rejected");
                                       errors.emplace_back(error);
                                   },
                           })
              .has_value(),
          "install callback hooks");

    const auto value = readCallback(lua.state(), "return function(n) return n + 1 end");
    check(value->beginCall(), "begin value callback");
    lua_pushinteger(lua.state(), 41);
    check(value->finishCall(1, 1), "host hook preserves callback results");
    check(lua_tointeger(lua.state(), -1) == 42, "callback result is preserved");
    lua_pop(lua.state(), 1);

    for (const auto code : {"return function() error('test error') end", "return function() error({}) end"}) {
        const auto failing = readCallback(lua.state(), code);
        check(failing->beginCall() && !failing->finishCall(0), "Lua errors are contained");
        check(lua_gettop(lua.state()) == 0, "error callback restores stack");
    }
    check(calls == 3 && errors.size() == 2, "all calls and errors use host hooks");
    check(errors[0].contains("test error") && errors[1] == "non-string Lua error", "error contents reach reporter");

    check(setCallbackHooks(lua.state(),
                           {
                               .protectedCall = [](lua_State*, int, int) -> int { throw std::runtime_error("host failure"); },
                               .reportError   = [&errors](std::string_view error) { errors.emplace_back(error); },
                           })
              .has_value(),
          "replace quiescent hooks");
    lua_pushinteger(lua.state(), 99);
    check(value->beginCall() && !value->finishCall(0), "host exceptions are contained");
    check(lua_gettop(lua.state()) == 1 && lua_tointeger(lua.state(), -1) == 99, "host failure preserves stack prefix");
    lua_pop(lua.state(), 1);
    check(errors.back() == "host failure", "host exception is reported");

    check(setCallbackHooks(lua.state(),
                           {
                               .protectedCall = [](lua_State*, int, int) { return LUA_OK; },
                               .reportError   = [&errors](std::string_view error) { errors.emplace_back(error); },
                           })
              .has_value(),
          "install invalid hook");
    check(value->beginCall() && !value->finishCall(0), "invalid host stack effects are rejected");
    check(lua_gettop(lua.state()) == 0 && errors.back().contains("stack contract"), "invalid hook restores the stack");

    check(setCallbackHooks(lua.state(), {}).has_value(), "restore standalone defaults");
    const auto empty = readCallback(lua.state(), "return function() end");
    check(empty->beginCall() && empty->finishCall(0), "default callback execution remains available");
}

static void nestedCallbackHooks() {
    CLuaState lua;
    lua.openLibs();
    registerAllBindings(lua.state());
    int        calls = 0;
    const auto inner = readCallback(lua.state(), "return function() end");
    const auto outer = readCallback(lua.state(), "return function() return 7 end");
    check(setCallbackHooks(lua.state(),
                           {
                               .protectedCall =
                                   [&calls, inner](lua_State* state, int arguments, int results) {
                                       if (++calls == 1)
                                           check(inner->beginCall() && inner->finishCall(0), "nested callback succeeds");
                                       return lua_pcall(state, arguments, results, 0);
                                   },
                           })
              .has_value(),
          "install nesting hook");
    check(outer->beginCall() && outer->finishCall(0, 1), "outer callback succeeds");
    check(calls == 2 && lua_tointeger(lua.state(), -1) == 7, "nested invocation preserves outer stack");
    lua_pop(lua.state(), 1);
}

static void hostWatchdog() {
    CLuaState lua;
    lua.openLibs();
    registerAllBindings(lua.state());
    std::string error;
    check(setCallbackHooks(lua.state(),
                           {
                               .protectedCall =
                                   [](lua_State* state, int arguments, int results) {
                                       lua_sethook(state, [](lua_State* state, lua_Debug*) { luaL_error(state, "host watchdog"); }, LUA_MASKCOUNT, 100);
                                       const int result = lua_pcall(state, arguments, results, 0);
                                       lua_sethook(state, nullptr, 0, 0);
                                       return result;
                                   },
                               .reportError = [&error](std::string_view message) { error = message; },
                           })
              .has_value(),
          "install host watchdog");
    const auto endless = readCallback(lua.state(), "return function() while true do end end");
    check(endless->beginCall() && !endless->finishCall(0), "host watchdog interrupts Lua");
    check(error.contains("host watchdog") && lua_gettop(lua.state()) == 0, "watchdog errors are contained");
}

static void hookLifetime() {
    CLuaState lua;
    registerAllBindings(lua.state());
    auto                     marker = std::make_shared<int>(0);
    const std::weak_ptr<int> weak   = marker;
    check(setCallbackHooks(lua.state(),
                           {
                               .reportError = [marker](std::string_view) { ++*marker; },
                           })
              .has_value(),
          "install retained host hook");
    marker.reset();
    check(!weak.expired(), "context retains host hook");
    check(retireState(lua.state()).has_value(), "retire host hooks");
    check(weak.expired(), "retirement releases host captures before lua_close");
    check(!setCallbackHooks(lua.state(), {}), "retired context rejects new hooks");
}

static void selfRemovingCallback() {
    CLuaState lua;
    lua.openLibs();
    registerAllBindings(lua.state());
    LuaCallback current;
    lua_pushlightuserdata(lua.state(), &current);
    lua_pushcclosure(
        lua.state(),
        [](lua_State* state) {
            sc<LuaCallback*>(lua_touserdata(state, lua_upvalueindex(1)))->reset();
            return 0;
        },
        1);
    lua_setglobal(lua.state(), "removeCallback");
    int errors = 0;
    check(setCallbackHooks(lua.state(),
                           {
                               .reportError = [&errors](std::string_view) { ++errors; },
                           })
              .has_value(),
          "install self-removal reporter");
    current = readCallback(lua.state(), "return function() removeCallback() end");
    check(current->beginCall() && current->finishCall(0), "self-removing callback succeeds");
    check(!current, "callback was destroyed during its own invocation");
    current = readCallback(lua.state(), "return function() removeCallback(); error('removed') end");
    check(current->beginCall() && !current->finishCall(0), "self-removing callback errors are contained");
    check(!current && errors == 1 && lua_gettop(lua.state()) == 0, "error handling survives callback destruction");
}

int main() {
    try {
        retirement();
        signalRetirement();
        retirementDuringCallback();
        rawStateClose();
        rawStateCloseOnAnotherThread();
        callbackHooks();
        nestedCallbackHooks();
        hostWatchdog();
        hookLifetime();
        selfRemovingCallback();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
