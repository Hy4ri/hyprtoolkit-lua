#include <hyprtoolkit-lua/LuaBindings.hpp>

#include "Binding.hpp"

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
}

#include <utility>

using namespace Hyprtoolkit::Lua;

static std::expected<void, std::string> result(lua_State* state, int status) {
    if (status == LUA_OK)
        return {};

    size_t      length = 0;
    const char* error  = lua_tolstring(state, -1, &length);
    std::string message{error ? error : "unknown Lua error", error ? length : 17};
    lua_pop(state, 1);
    return std::unexpected(std::move(message));
}

CLuaState::CLuaState() : m_state(luaL_newstate()) {
    if (!m_state)
        throw std::runtime_error("failed to create Lua state");
}

CLuaState::~CLuaState() {
    if (m_state) {
        closeContext(m_state);
        lua_close(m_state);
    }
}

CLuaState::CLuaState(CLuaState&& other) noexcept : m_state(std::exchange(other.m_state, nullptr)) {
    ;
}

CLuaState& CLuaState::operator=(CLuaState&& other) noexcept {
    if (this == &other)
        return *this;
    if (m_state) {
        closeContext(m_state);
        lua_close(m_state);
    }
    m_state = std::exchange(other.m_state, nullptr);
    return *this;
}

lua_State* CLuaState::state() {
    return m_state;
}

const lua_State* CLuaState::state() const {
    return m_state;
}

void CLuaState::openLibs() {
    luaL_openlibs(m_state);
}

std::expected<void, std::string> CLuaState::doFile(const std::string& path) {
    const int top        = lua_gettop(m_state);
    const int loadStatus = luaL_loadfile(m_state, path.c_str());
    if (loadStatus != LUA_OK)
        return result(m_state, loadStatus);
    auto execution = result(m_state, lua_pcall(m_state, 0, 0, 0));
    lua_settop(m_state, top);
    return execution;
}

std::expected<void, std::string> CLuaState::doString(const std::string& code) {
    const int top        = lua_gettop(m_state);
    const int loadStatus = luaL_loadbuffer(m_state, code.data(), code.size(), "string");
    if (loadStatus != LUA_OK)
        return result(m_state, loadStatus);
    auto execution = result(m_state, lua_pcall(m_state, 0, 0, 0));
    lua_settop(m_state, top);
    return execution;
}

bool CLuaState::has(const std::string& name) const {
    lua_getglobal(m_state, name.c_str());
    const bool exists = !lua_isnil(m_state, -1);
    lua_pop(m_state, 1);
    return exists;
}
