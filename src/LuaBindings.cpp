#include "Binding.hpp"

#include <hyprtoolkit-lua/LuaBindings.hpp>

#include <cstdio>
#include <cmath>
#include <format>
#include <limits>

using namespace Hyprtoolkit;
using namespace Hyprtoolkit::Lua;

static char                       CONTEXT_KEY       = 0;
static constexpr std::string_view CONTEXT_METATABLE = "hyprtoolkit.Context";

struct SContextHolder {
    std::shared_ptr<SLuaContext> context;
};

static int destroyContext(lua_State* state) {
    const auto storage = static_cast<SUserdata<SContextHolder>*>(luaL_testudata(state, 1, CONTEXT_METATABLE.data()));
    if (!storage || !storage->engaged)
        return 0;
    storage->engaged  = false;
    const auto holder = storage->value();
    if (!holder)
        return 0;
    holder->context->alive = false;
    holder->context->state = nullptr;
    std::destroy_at(holder);
    return 0;
}

std::shared_ptr<SLuaContext> Hyprtoolkit::Lua::context(lua_State* state) {
    lua_rawgetp(state, LUA_REGISTRYINDEX, &CONTEXT_KEY);
    if (const auto holder = testObject<SContextHolder>(state, -1, CONTEXT_METATABLE); holder) {
        auto result = holder->context;
        lua_pop(state, 1);
        return result;
    }
    lua_pop(state, 1);

    lua_rawgeti(state, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
    auto luaContext   = std::make_shared<SLuaContext>();
    luaContext->state = lua_tothread(state, -1);
    lua_pop(state, 1);
    if (luaL_newmetatable(state, CONTEXT_METATABLE.data())) {
        lua_pushcfunction(state, destroyContext);
        lua_setfield(state, -2, "__gc");
        lua_pushliteral(state, "hyprtoolkit.Context");
        lua_setfield(state, -2, "__metatable");
    }
    lua_pop(state, 1);

    pushObject(state, SContextHolder{.context = luaContext}, CONTEXT_METATABLE);
    lua_pushvalue(state, -1);
    lua_rawsetp(state, LUA_REGISTRYINDEX, &CONTEXT_KEY);
    lua_pop(state, 1);
    return luaContext;
}

void Hyprtoolkit::Lua::closeContext(lua_State* state) {
    lua_rawgetp(state, LUA_REGISTRYINDEX, &CONTEXT_KEY);
    if (const auto holder = testObject<SContextHolder>(state, -1, CONTEXT_METATABLE); holder) {
        holder->context->alive = false;
        holder->context->state = nullptr;
    }
    lua_pop(state, 1);
}

void Hyprtoolkit::Lua::requireBackendActive(lua_State* state) {
    const auto luaContext = context(state);
    if (!luaContext->backendActive)
        fail(luaContext->backendCreated ? "backend has been destroyed" : "a backend must be created first");
}

CLuaCallback::CLuaCallback(lua_State* state, int index) : m_context(context(state)) {
    lua_pushvalue(state, lua_absindex(state, index));
    m_reference = luaL_ref(state, LUA_REGISTRYINDEX);
}

CLuaCallback::~CLuaCallback() {
    if (!m_context->alive || !m_context->state || m_reference == LUA_NOREF)
        return;
    if (m_context->ownerThread != std::this_thread::get_id())
        return;
    luaL_unref(m_context->state, LUA_REGISTRYINDEX, m_reference);
}

void CLuaCallback::release() {
    if (m_reference == LUA_NOREF)
        return;
    if (m_context->alive && m_context->state && m_context->ownerThread == std::this_thread::get_id())
        luaL_unref(m_context->state, LUA_REGISTRYINDEX, m_reference);
    m_reference = LUA_NOREF;
}

lua_State* CLuaCallback::state() const {
    return m_context->state;
}

bool CLuaCallback::beginCall() const {
    if (!m_context->alive || !m_context->state || m_reference == LUA_NOREF || m_context->ownerThread != std::this_thread::get_id())
        return false;
    lua_rawgeti(m_context->state, LUA_REGISTRYINDEX, m_reference);
    return true;
}

bool CLuaCallback::finishCall(int argumentCount, int resultCount) const {
    if (!m_context->alive || !m_context->state)
        return false;
    if (lua_pcall(m_context->state, argumentCount, resultCount, 0) == LUA_OK)
        return true;

    size_t      length = 0;
    const char* error  = lua_tolstring(m_context->state, -1, &length);
    if (error)
        std::fprintf(stderr, "hyprtoolkit-lua callback error: %.*s\n", static_cast<int>(length), error);
    else
        std::fprintf(stderr, "hyprtoolkit-lua callback error: unknown error\n");
    lua_pop(m_context->state, 1);
    return false;
}

[[noreturn]] void Hyprtoolkit::Lua::fail(std::string message) {
    throw std::runtime_error(std::move(message));
}

void Hyprtoolkit::Lua::expectArguments(lua_State* state, int minimum, int maximum) {
    const int count = lua_gettop(state);
    if (count >= minimum && (maximum < 0 || count <= maximum))
        return;
    if (maximum < 0)
        fail(std::format("expected {} or more arguments, got {}", minimum, count));
    if (maximum == minimum)
        fail(std::format("expected {} arguments, got {}", minimum, count));
    fail(std::format("expected {} to {} arguments, got {}", minimum, maximum, count));
}

bool Hyprtoolkit::Lua::boolean(lua_State* state, int index) {
    if (!lua_isboolean(state, index))
        fail("argument " + std::to_string(index) + " must be a boolean");
    return lua_toboolean(state, index);
}

double Hyprtoolkit::Lua::number(lua_State* state, int index) {
    if (lua_type(state, index) != LUA_TNUMBER)
        fail("argument " + std::to_string(index) + " must be a number");
    const auto value = lua_tonumber(state, index);
    if (!std::isfinite(value))
        fail("argument " + std::to_string(index) + " must be finite");
    return value;
}

float Hyprtoolkit::Lua::floatNumber(lua_State* state, int index) {
    const auto value = number(state, index);
    if (value < std::numeric_limits<float>::lowest() || value > std::numeric_limits<float>::max())
        fail("argument " + std::to_string(index) + " is outside the float range");
    return sc<float>(value);
}

int Hyprtoolkit::Lua::intNumber(lua_State* state, int index) {
    const auto value = integer(state, index);
    if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
        fail("argument " + std::to_string(index) + " is outside the integer range");
    return sc<int>(value);
}

lua_Integer Hyprtoolkit::Lua::integer(lua_State* state, int index) {
    if (!lua_isinteger(state, index))
        fail("argument " + std::to_string(index) + " must be an integer");
    return lua_tointeger(state, index);
}

std::string Hyprtoolkit::Lua::string(lua_State* state, int index) {
    if (lua_type(state, index) != LUA_TSTRING)
        fail("argument " + std::to_string(index) + " must be a string");
    size_t      length = 0;
    const char* value  = lua_tolstring(state, index, &length);
    return {value, length};
}

bool Hyprtoolkit::Lua::isNoneOrNil(lua_State* state, int index) {
    return lua_isnoneornil(state, index);
}

LuaCallback Hyprtoolkit::Lua::callback(lua_State* state, int index, bool optional) {
    if (optional && lua_isnoneornil(state, index))
        return {};
    if (!lua_isfunction(state, index))
        fail("argument " + std::to_string(index) + " must be a function");
    return std::make_shared<CLuaCallback>(state, index);
}

void Hyprtoolkit::Lua::registerMetatable(lua_State* state, std::string_view name, const luaL_Reg* methods, lua_CFunction gc) {
    if (!luaL_newmetatable(state, name.data())) {
        lua_pop(state, 1);
        return;
    }
    lua_pushlstring(state, name.data(), name.size());
    lua_pushcclosure(state, gc, 1);
    lua_setfield(state, -2, "__gc");

    lua_newtable(state);
    for (const luaL_Reg* method = methods; method->name; ++method) {
        lua_pushcfunction(state, method->func);
        if (method->name[0] == '_' && method->name[1] == '_')
            lua_setfield(state, -3, method->name);
        else
            lua_setfield(state, -2, method->name);
    }
    lua_setfield(state, -2, "__methods");

    lua_getfield(state, -1, "__index");
    const bool customIndex = !lua_isnil(state, -1);
    lua_pop(state, 1);
    if (!customIndex) {
        lua_getfield(state, -1, "__methods");
        lua_setfield(state, -2, "__index");
    }

    lua_pushlstring(state, name.data(), name.size());
    lua_setfield(state, -2, "__metatable");
    lua_pop(state, 1);
}

void Hyprtoolkit::Lua::setFunction(lua_State* state, int table, const char* name, lua_CFunction function) {
    table = lua_absindex(state, table);
    lua_pushcfunction(state, function);
    lua_setfield(state, table, name);
}

void Hyprtoolkit::Lua::setInteger(lua_State* state, int table, const char* name, lua_Integer value) {
    table = lua_absindex(state, table);
    lua_pushinteger(state, value);
    lua_setfield(state, table, name);
}

void Hyprtoolkit::Lua::setClass(lua_State* state, int module, const char* name, const luaL_Reg* functions) {
    module = lua_absindex(state, module);
    lua_newtable(state);
    luaL_setfuncs(state, functions, 0);
    lua_setfield(state, module, name);
}

void Hyprtoolkit::Lua::pushConnection(lua_State* state, Hyprutils::Signal::CHyprSignalListener listener) {
    pushObject(state, SConnection{.listener = std::move(listener)}, MT_CONNECTION);
}

static int disconnect(lua_State* state) {
    object<SConnection>(state, 1, MT_CONNECTION).listener.reset();
    return 0;
}

static int connected(lua_State* state) {
    lua_pushboolean(state, !!object<SConnection>(state, 1, MT_CONNECTION).listener);
    return 1;
}

static constexpr luaL_Reg CONNECTION_METHODS[] = {
    {"disconnect", protect<disconnect>},
    {"connected", protect<connected>},
    {nullptr, nullptr},
};

void Hyprtoolkit::Lua::registerAllBindings(lua_State* state) {
    createModule(state);
    lua_pushvalue(state, -1);
    lua_setglobal(state, "hyprtoolkit");
    lua_pop(state, 1);
}

int Hyprtoolkit::Lua::createModule(lua_State* state) {
    context(state);
    registerMetatable(state, MT_CONNECTION, CONNECTION_METHODS, destroyObject<SConnection>);

    lua_newtable(state);
    const int module = lua_gettop(state);
    registerTypes(state, module);
    registerElements(state, module);
    registerElementBuilders(state, module);
    registerCore(state, module);
    registerWindow(state, module);

    lua_pushliteral(state, HYPRTOOLKIT_LUA_VERSION);
    lua_setfield(state, module, "version");
    return 1;
}

int Hyprtoolkit::Lua::openModule(lua_State* state) noexcept {
    return protect<createModule>(state);
}

SP<CLuaState> Hyprtoolkit::Lua::createLuaState() {
    return makeShared<CLuaState>();
}
