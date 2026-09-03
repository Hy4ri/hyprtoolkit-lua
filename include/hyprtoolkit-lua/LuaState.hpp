#pragma once

#include <expected>
#include <string>

struct lua_State;

namespace Hyprtoolkit::Lua {

    class CLuaState {
      public:
        CLuaState();
        ~CLuaState();

        CLuaState(const CLuaState&)            = delete;
        CLuaState& operator=(const CLuaState&) = delete;
        CLuaState(CLuaState&& other) noexcept;
        CLuaState&                       operator=(CLuaState&& other) noexcept;

        lua_State*                       state();
        const lua_State*                 state() const;

        void                             openLibs();
        std::expected<void, std::string> doFile(const std::string& path);
        std::expected<void, std::string> doString(const std::string& code);

        bool                             has(const std::string& name) const;

      private:
        lua_State* m_state = nullptr;
    };

} // namespace Hyprtoolkit::Lua
