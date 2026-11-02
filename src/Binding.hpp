#pragma once

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <hyprtoolkit/core/Animation.hpp>
#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/core/Output.hpp>
#include <hyprtoolkit/core/SessionLock.hpp>
#include <hyprtoolkit/core/Timer.hpp>
#include <hyprtoolkit/element/Button.hpp>
#include <hyprtoolkit/element/Checkbox.hpp>
#include <hyprtoolkit/element/ColumnLayout.hpp>
#include <hyprtoolkit/element/Combobox.hpp>
#include <hyprtoolkit/element/Image.hpp>
#include <hyprtoolkit/element/Line.hpp>
#include <hyprtoolkit/element/Null.hpp>
#include <hyprtoolkit/element/ProgressBar.hpp>
#include <hyprtoolkit/element/RadioGroup.hpp>
#include <hyprtoolkit/element/Rectangle.hpp>
#include <hyprtoolkit/element/RowLayout.hpp>
#include <hyprtoolkit/element/ScrollArea.hpp>
#include <hyprtoolkit/element/Slider.hpp>
#include <hyprtoolkit/element/Spinbox.hpp>
#include <hyprtoolkit/element/Text.hpp>
#include <hyprtoolkit/element/Textbox.hpp>
#include <hyprtoolkit/palette/Color.hpp>
#include <hyprtoolkit/palette/Gradient.hpp>
#include <hyprtoolkit/palette/Palette.hpp>
#include <hyprtoolkit/system/Icons.hpp>
#include <hyprtoolkit/window/Window.hpp>
#include <hyprutils/signal/Listener.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprtoolkit-lua/Embedding.hpp>

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

namespace Hyprtoolkit::Lua {

    using namespace Hyprutils::Memory;

    template <typename T>
    using SP = Hyprutils::Memory::CSharedPointer<T>;

    template <typename T>
    using WP = Hyprutils::Memory::CWeakPointer<T>;

    inline constexpr std::string_view MT_COLOR          = "hyprtoolkit.Color";
    inline constexpr std::string_view MT_SIZE           = "hyprtoolkit.DynamicSize";
    inline constexpr std::string_view MT_FONT_SIZE      = "hyprtoolkit.FontSize";
    inline constexpr std::string_view MT_ELEMENT        = "hyprtoolkit.Element";
    inline constexpr std::string_view MT_BUILDER        = "hyprtoolkit.Builder";
    inline constexpr std::string_view MT_BACKEND        = "hyprtoolkit.Backend";
    inline constexpr std::string_view MT_TIMER          = "hyprtoolkit.Timer";
    inline constexpr std::string_view MT_WINDOW         = "hyprtoolkit.Window";
    inline constexpr std::string_view MT_WINDOW_BUILDER = "hyprtoolkit.WindowBuilder";
    inline constexpr std::string_view MT_OUTPUT         = "hyprtoolkit.Output";
    inline constexpr std::string_view MT_ICON_FACTORY   = "hyprtoolkit.IconFactory";
    inline constexpr std::string_view MT_ICON           = "hyprtoolkit.Icon";
    inline constexpr std::string_view MT_PALETTE        = "hyprtoolkit.Palette";
    inline constexpr std::string_view MT_SESSION_LOCK   = "hyprtoolkit.SessionLock";
    inline constexpr std::string_view MT_RADIO_GROUP    = "hyprtoolkit.RadioGroup";
    inline constexpr std::string_view MT_CONNECTION     = "hyprtoolkit.Connection";

    class CLuaCallback;
    struct SConnection;

    struct SLuaContext {
        lua_State*                               state          = nullptr;
        std::atomic_bool                         alive          = true;
        std::thread::id                          ownerThread    = std::this_thread::get_id();
        bool                                     backendCreated = false;
        bool                                     backendActive  = false;
        bool                                     retiring       = false;
        size_t                                   activeCalls    = 0;
        SCallbackHooks                           hooks;

        std::vector<std::weak_ptr<CLuaCallback>> callbacks;
        std::vector<WP<SConnection>>             connections;
    };

    class CLuaCallback {
      public:
        CLuaCallback(lua_State* state, int index);
        ~CLuaCallback();

        CLuaCallback(const CLuaCallback&)            = delete;
        CLuaCallback& operator=(const CLuaCallback&) = delete;

        bool          beginCall() const;
        bool          finishCall(int argumentCount, int resultCount = 0) const;
        void          reportError(std::string_view message) const;
        void          release();
        lua_State*    state() const;

      private:
        std::shared_ptr<SLuaContext> m_context;
        int                          m_reference = LUA_NOREF;
    };

    using LuaCallback = std::shared_ptr<CLuaCallback>;

    struct SPendingCallback {
        LuaCallback callback;
    };

    using TimerPointer = CAtomicSharedPointer<CTimer>;

    struct STimer {
        TimerPointer                      timer;
        std::shared_ptr<SPendingCallback> pending;
    };

    using SBuilderVariant = std::variant<SP<CTextBuilder>, SP<CRectangleBuilder>, SP<CCheckboxBuilder>, SP<CProgressBarBuilder>, SP<CScrollAreaBuilder>, SP<CSpinboxBuilder>,
                                         SP<CSliderBuilder>, SP<CComboboxBuilder>, SP<CButtonBuilder>, SP<CTextboxBuilder>, SP<CRowLayoutBuilder>, SP<CImageBuilder>,
                                         SP<CColumnLayoutBuilder>, SP<CNullBuilder>, SP<CLineBuilder>>;

    struct SConnection {
        Hyprutils::Signal::CHyprSignalListener listener;
    };

    struct SBackend {
        SP<IBackend>                                 backend;
        std::vector<std::weak_ptr<SPendingCallback>> pendingCallbacks;
        std::shared_ptr<SPendingCallback>            logCallback;
        std::weak_ptr<SLuaContext>                   context;
        bool                                         active = true;
    };

    std::shared_ptr<SLuaContext>           context(lua_State* state);
    std::shared_ptr<SLuaContext>           findContext(lua_State* state);
    void                                   closeContext(lua_State* state);
    void                                   requireBackendActive(lua_State* state);

    [[noreturn]] void                      fail(std::string message);
    void                                   expectArguments(lua_State* state, int minimum, int maximum = -1);
    bool                                   boolean(lua_State* state, int index);
    double                                 number(lua_State* state, int index);
    float                                  floatNumber(lua_State* state, int index);
    int                                    intNumber(lua_State* state, int index);
    lua_Integer                            integer(lua_State* state, int index);
    std::string                            string(lua_State* state, int index);
    bool                                   isNoneOrNil(lua_State* state, int index);
    LuaCallback                            callback(lua_State* state, int index, bool optional = false);

    Hyprutils::Math::Vector2D              vector(lua_State* state, int index);
    void                                   pushVector(lua_State* state, const Hyprutils::Math::Vector2D& value);
    CDynamicSize                           dynamicSize(lua_State* state, int index);
    CFontSize                              fontSize(lua_State* state, int index);
    CHyprColor                             color(lua_State* state, int index);
    CGradientValueData                     gradient(lua_State* state, int index);
    SAnimation                             animation(lua_State* state, int index);
    std::vector<std::string>               stringArray(lua_State* state, int index);
    std::vector<Hyprutils::Math::Vector2D> vectorArray(lua_State* state, int index);

    void                                   pushKeyboardEvent(lua_State* state, const Input::SKeyboardKeyEvent& event);
    void                                   pushTouchEvent(lua_State* state, const Input::STouchEvent& event);

    template <typename T>
    struct alignas(T) SUserdata {
        bool engaged = false;
        alignas(T) std::array<std::byte, sizeof(T)> storage;

        T* value() {
            return std::launder(reinterpret_cast<T*>(storage.data()));
        }
    };

    template <typename T>
    T& object(lua_State* state, int index, std::string_view metatable) {
        const auto ptr = luaL_testudata(state, index, metatable.data());
        if (!ptr)
            fail("argument " + std::to_string(index) + " must be a " + std::string{metatable});
        const auto userdata = static_cast<SUserdata<T>*>(ptr);
        if (!userdata->engaged)
            fail("argument " + std::to_string(index) + " is a finalized " + std::string{metatable});
        return *userdata->value();
    }

    template <typename T>
    T* testObject(lua_State* state, int index, std::string_view metatable) {
        const auto ptr = luaL_testudata(state, index, metatable.data());
        if (!ptr)
            return nullptr;
        const auto userdata = static_cast<SUserdata<T>*>(ptr);
        return userdata->engaged ? userdata->value() : nullptr;
    }

    template <typename T>
    int destroyObject(lua_State* state) {
        const auto metatable = lua_tostring(state, lua_upvalueindex(1));
        if (!metatable)
            return 0;
        const auto userdata = static_cast<SUserdata<T>*>(luaL_testudata(state, 1, metatable));
        if (userdata && userdata->engaged) {
            userdata->engaged = false;
            std::destroy_at(userdata->value());
        }
        return 0;
    }

    template <typename T>
    void pushObject(lua_State* state, T value, std::string_view metatable) {
        const auto storage  = lua_newuserdatauv(state, sizeof(SUserdata<T>), 0);
        const auto userdata = std::construct_at(static_cast<SUserdata<T>*>(storage));
        luaL_getmetatable(state, metatable.data());
        lua_setmetatable(state, -2);
        std::construct_at(userdata->value(), std::move(value));
        userdata->engaged = true;
    }

    template <int (*Function)(lua_State*)>
    int protect(lua_State* state) noexcept {
        char message[1024] = {};
        try {
            return Function(state);
        } catch (const std::exception& error) { std::snprintf(message, sizeof(message), "%s", error.what()); } catch (...) {
            std::memcpy(message, "unknown C++ exception", sizeof("unknown C++ exception"));
        }
        lua_pushstring(state, message);
        return lua_error(state);
    }

    void         registerMetatable(lua_State* state, std::string_view name, const luaL_Reg* methods, lua_CFunction gc);
    void         setFunction(lua_State* state, int table, const char* name, lua_CFunction function);
    void         setInteger(lua_State* state, int table, const char* name, lua_Integer value);
    void         setClass(lua_State* state, int module, const char* name, const luaL_Reg* functions);

    SP<IElement> checkElement(lua_State* state, int index);
    void         pushElement(lua_State* state, SP<IElement> element);
    void         pushBuilder(lua_State* state, SBuilderVariant builder);
    void         pushWindow(lua_State* state, SP<IWindow> window);
    void         pushOutput(lua_State* state, SP<IOutput> output);
    void         pushIcon(lua_State* state, SP<ISystemIconDescription> icon);
    void         pushConnection(lua_State* state, Hyprutils::Signal::CHyprSignalListener listener);

    void         registerTypes(lua_State* state, int module);
    void         registerElements(lua_State* state, int module);
    void         registerElementBuilders(lua_State* state, int module);
    void         registerCore(lua_State* state, int module);
    void         registerWindow(lua_State* state, int module);

}
