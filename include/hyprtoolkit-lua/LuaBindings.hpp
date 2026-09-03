#pragma once

#include <hyprutils/memory/SharedPtr.hpp>

#include "LuaState.hpp"

namespace Hyprtoolkit::Lua {

    void                                         registerAllBindings(lua_State* state);
    int                                          createModule(lua_State* state);
    int                                          openModule(lua_State* state) noexcept;

    Hyprutils::Memory::CSharedPointer<CLuaState> createLuaState();

} // namespace Hyprtoolkit::Lua
