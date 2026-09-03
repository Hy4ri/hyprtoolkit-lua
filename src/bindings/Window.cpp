#include "../Binding.hpp"

#include <limits>

using namespace Hyprtoolkit;
using namespace Hyprtoolkit::Lua;
using namespace Hyprutils::Memory;

static SP<CWindowBuilder> windowBuilder(lua_State* state, int index) {
    const auto builder = object<SP<CWindowBuilder>>(state, index, MT_WINDOW_BUILDER);
    if (!builder)
        fail("argument " + std::to_string(index) + " is an empty WindowBuilder");
    return builder;
}

static SP<IWindow> window(lua_State* state, int index) {
    requireBackendActive(state);
    const auto value = object<SP<IWindow>>(state, index, MT_WINDOW);
    if (!value)
        fail("argument " + std::to_string(index) + " is an empty Window");
    return value;
}

static SP<IOutput> output(lua_State* state, int index) {
    requireBackendActive(state);
    const auto value = object<SP<IOutput>>(state, index, MT_OUTPUT);
    if (!value)
        fail("argument " + std::to_string(index) + " is an empty Output");
    return value;
}

static uint32_t unsigned32(lua_State* state, int index, const char* name) {
    const auto value = integer(state, index);
    if (value < 0 || sc<uint64_t>(value) > std::numeric_limits<uint32_t>::max())
        fail("argument " + std::to_string(index) + " must be a valid " + name);
    return sc<uint32_t>(value);
}

static int32_t signed32(lua_State* state, int index, const char* name) {
    const auto value = integer(state, index);
    if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max())
        fail("argument " + std::to_string(index) + " must be a valid " + name);
    return sc<int32_t>(value);
}

static eWindowType windowType(lua_State* state, int index) {
    const auto value = integer(state, index);
    if (value < HT_WINDOW_TOPLEVEL || value > HT_WINDOW_LOCK_SURFACE)
        fail("argument " + std::to_string(index) + " is not a valid window type");
    return sc<eWindowType>(value);
}

static eResizeEdge resizeEdge(lua_State* state, int index) {
    const auto value = integer(state, index);
    switch (value) {
        case HT_RESIZE_EDGE_NONE:
        case HT_RESIZE_EDGE_TOP:
        case HT_RESIZE_EDGE_BOTTOM:
        case HT_RESIZE_EDGE_LEFT:
        case HT_RESIZE_EDGE_TOP_LEFT:
        case HT_RESIZE_EDGE_BOTTOM_LEFT:
        case HT_RESIZE_EDGE_RIGHT:
        case HT_RESIZE_EDGE_TOP_RIGHT:
        case HT_RESIZE_EDGE_BOTTOM_RIGHT: return sc<eResizeEdge>(value);
        default: fail("argument " + std::to_string(index) + " is not a valid resize edge");
    }
}

template <typename Apply>
static int configureBuilder(lua_State* state, Apply&& apply) {
    expectArguments(state, 2, 2);
    const auto builder = windowBuilder(state, 1);
    apply(builder);
    lua_pushvalue(state, 1);
    return 1;
}

static int beginWindowBuilder(lua_State* state) {
    expectArguments(state, 0, 0);
    pushObject(state, CWindowBuilder::begin(), MT_WINDOW_BUILDER);
    return 1;
}

static int setWindowType(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->type(windowType(state, 2)); });
}

static int setAppTitle(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->appTitle(string(state, 2)); });
}

static int setAppClass(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->appClass(string(state, 2)); });
}

static int setPreferredSize(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->preferredSize(vector(state, 2)); });
}

static int setMinSize(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->minSize(vector(state, 2)); });
}

static int setMaxSize(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->maxSize(vector(state, 2)); });
}

static int setResizable(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->resizable(boolean(state, 2)); });
}

static int setAutosize(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->autosize(boolean(state, 2)); });
}

static int setPreferredOutput(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->prefferedOutput(output(state, 2)); });
}

static int setMarginTopLeft(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->marginTopLeft(vector(state, 2)); });
}

static int setMarginBottomRight(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->marginBottomRight(vector(state, 2)); });
}

static int setLayer(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) {
        const auto value = unsigned32(state, 2, "layer");
        if (value > 3)
            fail("argument 2 is not a valid layer");
        builder->layer(value);
    });
}

static int setAnchor(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) {
        const auto value = unsigned32(state, 2, "layer anchor");
        if ((value & ~15U) != 0)
            fail("argument 2 is not a valid layer anchor");
        builder->anchor(value);
    });
}

static int setExclusiveEdge(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) {
        const auto value = unsigned32(state, 2, "exclusive edge");
        if (value != 0 && value != 1 && value != 2 && value != 4 && value != 8)
            fail("argument 2 is not a valid exclusive edge");
        builder->exclusiveEdge(value);
    });
}

static int setExclusiveZone(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->exclusiveZone(signed32(state, 2, "exclusive zone")); });
}

static int setKbInteractive(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) {
        const auto value = unsigned32(state, 2, "keyboard interactivity mode");
        if (value > 2)
            fail("argument 2 is not a valid keyboard interactivity mode");
        builder->kbInteractive(value);
    });
}

static int setInhibitShortcuts(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->inhibitShortcuts(boolean(state, 2)); });
}

static int setParent(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->parent(window(state, 2)); });
}

static int setPosition(lua_State* state) {
    return configureBuilder(state, [&](const auto& builder) { builder->pos(vector(state, 2)); });
}

static int commenceWindow(lua_State* state) {
    expectArguments(state, 1, 1);
    requireBackendActive(state);
    pushWindow(state, windowBuilder(state, 1)->commence());
    return 1;
}

void Hyprtoolkit::Lua::pushWindow(lua_State* state, SP<IWindow> value) {
    if (!value) {
        lua_pushnil(state);
        return;
    }
    pushObject(state, std::move(value), MT_WINDOW);
}

static int windowPixelSize(lua_State* state) {
    expectArguments(state, 1, 1);
    pushVector(state, window(state, 1)->pixelSize());
    return 1;
}

static int windowScale(lua_State* state) {
    expectArguments(state, 1, 1);
    lua_pushnumber(state, window(state, 1)->scale());
    return 1;
}

static int windowClose(lua_State* state) {
    expectArguments(state, 1, 1);
    window(state, 1)->close();
    return 0;
}

static int windowOpen(lua_State* state) {
    expectArguments(state, 1, 1);
    window(state, 1)->open();
    return 0;
}

static int windowCursorPos(lua_State* state) {
    expectArguments(state, 1, 1);
    pushVector(state, window(state, 1)->cursorPos());
    return 1;
}

static int windowSetSize(lua_State* state) {
    expectArguments(state, 2, 2);
    window(state, 1)->setSize(vector(state, 2));
    return 0;
}

static int windowStartInteractiveResize(lua_State* state) {
    expectArguments(state, 2, 2);
    window(state, 1)->startInteractiveResize(resizeEdge(state, 2));
    return 0;
}

static int windowRoot(lua_State* state) {
    expectArguments(state, 1, 1);
    pushElement(state, window(state, 1)->m_rootElement);
    return 1;
}

static int windowOnResized(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto value = window(state, 1);
    const auto cb    = callback(state, 2);
    pushConnection(state, value->m_events.resized.listen([cb](const Hyprutils::Math::Vector2D& size) {
        if (!cb->beginCall())
            return;
        pushVector(cb->state(), size);
        cb->finishCall(1);
    }));
    return 1;
}

static int windowOnCloseRequest(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto value = window(state, 1);
    const auto cb    = callback(state, 2);
    pushConnection(state, value->m_events.closeRequest.listen([cb] {
        if (!cb->beginCall())
            return;
        cb->finishCall(0);
    }));
    return 1;
}

static int windowOnPopupClosed(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto value = window(state, 1);
    const auto cb    = callback(state, 2);
    pushConnection(state, value->m_events.popupClosed.listen([cb] {
        if (!cb->beginCall())
            return;
        cb->finishCall(0);
    }));
    return 1;
}

static int windowOnLayerClosed(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto value = window(state, 1);
    const auto cb    = callback(state, 2);
    pushConnection(state, value->m_events.layerClosed.listen([cb] {
        if (!cb->beginCall())
            return;
        cb->finishCall(0);
    }));
    return 1;
}

static int windowOnKeyboardKey(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto value = window(state, 1);
    const auto cb    = callback(state, 2);
    pushConnection(state, value->m_events.keyboardKey.listen([cb](const Input::SKeyboardKeyEvent& event) {
        if (!cb->beginCall())
            return;
        pushKeyboardEvent(cb->state(), event);
        cb->finishCall(1);
    }));
    return 1;
}

static constexpr luaL_Reg WINDOW_BUILDER_METHODS[] = {
    {"type", protect<setWindowType>},
    {"appTitle", protect<setAppTitle>},
    {"appClass", protect<setAppClass>},
    {"preferredSize", protect<setPreferredSize>},
    {"minSize", protect<setMinSize>},
    {"maxSize", protect<setMaxSize>},
    {"resizable", protect<setResizable>},
    {"autosize", protect<setAutosize>},
    {"preferredOutput", protect<setPreferredOutput>},
    {"prefferedOutput", protect<setPreferredOutput>},
    {"marginTopLeft", protect<setMarginTopLeft>},
    {"marginBottomRight", protect<setMarginBottomRight>},
    {"layer", protect<setLayer>},
    {"anchor", protect<setAnchor>},
    {"exclusiveEdge", protect<setExclusiveEdge>},
    {"exclusiveZone", protect<setExclusiveZone>},
    {"kbInteractive", protect<setKbInteractive>},
    {"inhibitShortcuts", protect<setInhibitShortcuts>},
    {"parent", protect<setParent>},
    {"pos", protect<setPosition>},
    {"commence", protect<commenceWindow>},
    {nullptr, nullptr},
};

static constexpr luaL_Reg WINDOW_BUILDER_CLASS[] = {
    {"begin", protect<beginWindowBuilder>},
    {nullptr, nullptr},
};

static constexpr luaL_Reg WINDOW_METHODS[] = {
    {"pixelSize", protect<windowPixelSize>},
    {"scale", protect<windowScale>},
    {"close", protect<windowClose>},
    {"open", protect<windowOpen>},
    {"cursorPos", protect<windowCursorPos>},
    {"setSize", protect<windowSetSize>},
    {"startInteractiveResize", protect<windowStartInteractiveResize>},
    {"root", protect<windowRoot>},
    {"rootElement", protect<windowRoot>},
    {"onResized", protect<windowOnResized>},
    {"onCloseRequest", protect<windowOnCloseRequest>},
    {"onPopupClosed", protect<windowOnPopupClosed>},
    {"onLayerClosed", protect<windowOnLayerClosed>},
    {"onKeyboardKey", protect<windowOnKeyboardKey>},
    {nullptr, nullptr},
};

void Hyprtoolkit::Lua::registerWindow(lua_State* state, int module) {
    registerMetatable(state, MT_WINDOW_BUILDER, WINDOW_BUILDER_METHODS, protect<destroyObject<SP<CWindowBuilder>>>);
    registerMetatable(state, MT_WINDOW, WINDOW_METHODS, protect<destroyObject<SP<IWindow>>>);
    setClass(state, module, "WindowBuilder", WINDOW_BUILDER_CLASS);
}
