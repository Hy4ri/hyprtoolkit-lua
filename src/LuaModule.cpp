#include <hyprtoolkit-lua/LuaBindings.hpp>

#if defined(__GNUC__)
#define HYPRTOOLKIT_LUA_EXPORT __attribute__((visibility("default")))
#else
#define HYPRTOOLKIT_LUA_EXPORT
#endif

extern "C" HYPRTOOLKIT_LUA_EXPORT int luaopen_hyprtoolkit(lua_State* state) {
    return Hyprtoolkit::Lua::openModule(state);
}
