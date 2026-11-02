#include "../Binding.hpp"

#include <limits>

using namespace Hyprtoolkit;
using namespace Hyprtoolkit::Lua;
using namespace Hyprutils::Memory;

template <typename T>
static SP<T> concreteElement(lua_State* state, int index, const char* name) {
    const auto element  = checkElement(state, index);
    const auto concrete = dynamicPointerCast<T>(element);
    if (!concrete)
        fail("argument " + std::to_string(index) + " must be a " + name + " element");
    return concrete;
}

static size_t luaIndex(lua_State* state, int index) {
    const lua_Integer value = integer(state, index);
    if (value < 1)
        fail("argument " + std::to_string(index) + " must be a positive index");
    return sc<size_t>(value - 1);
}

static void pushIndex(lua_State* state, size_t value) {
    if (value == std::numeric_limits<size_t>::max()) {
        lua_pushnil(state);
        return;
    }
    if (value >= sc<size_t>(std::numeric_limits<lua_Integer>::max()))
        fail("element index is too large for Lua");
    lua_pushinteger(state, sc<lua_Integer>(value) + 1);
}

SP<IElement> Hyprtoolkit::Lua::checkElement(lua_State* state, int index) {
    requireBackendActive(state);
    const auto element = object<SP<IElement>>(state, index, MT_ELEMENT);
    if (!element)
        fail("argument " + std::to_string(index) + " is an empty element");
    return element;
}

void Hyprtoolkit::Lua::pushElement(lua_State* state, SP<IElement> element) {
    const auto existing = findContext(state);
    if (!existing || !existing->alive || existing->retiring || existing->ownerThread != std::this_thread::get_id())
        fail("element bindings are not active on this thread");
    luaL_getmetatable(state, MT_ELEMENT.data());
    const bool registered = lua_istable(state, -1);
    lua_pop(state, 1);
    if (!registered)
        fail("element bindings have not been registered");
    if (!element) {
        lua_pushnil(state);
        return;
    }
    requireBackendActive(state);
    pushObject(state, std::move(element), MT_ELEMENT);
}

std::expected<SP<IElement>, std::string> Hyprtoolkit::Lua::getElement(lua_State* state, int index) {
    const auto existing = findContext(state);
    if (!existing || !existing->alive || existing->retiring || existing->ownerThread != std::this_thread::get_id())
        return std::unexpected("element bindings are not active on this thread");
    if (!existing->backendActive || (existing->backendMode == eBackendMode::HOST && existing->hostBackend.expired()))
        return std::unexpected("backend is not active");
    if (lua_isnil(state, index))
        return SP<IElement>{};
    const auto element = testObject<SP<IElement>>(state, index, MT_ELEMENT);
    if (!element || !*element)
        return std::unexpected("expected a live hyprtoolkit element");
    return *element;
}

static int elementSize(lua_State* state) {
    expectArguments(state, 1, 1);
    pushVector(state, checkElement(state, 1)->size());
    return 1;
}

static int elementPosFromParent(lua_State* state) {
    expectArguments(state, 1, 1);
    pushVector(state, checkElement(state, 1)->posFromParent());
    return 1;
}

static int elementSetPositionMode(lua_State* state) {
    expectArguments(state, 2, 2);
    const lua_Integer mode = integer(state, 2);
    if (mode < IElement::HT_POSITION_ABSOLUTE || mode > IElement::HT_POSITION_AUTO)
        fail("argument 2 is not a valid position mode");
    checkElement(state, 1)->setPositionMode(sc<IElement::ePositionMode>(mode));
    return 0;
}

static int elementSetPositionFlag(lua_State* state) {
    expectArguments(state, 3, 3);
    const lua_Integer flag = integer(state, 2);
    if (flag < 0 || flag > IElement::HT_POSITION_FLAG_ALL)
        fail("argument 2 is not a valid position flag");
    checkElement(state, 1)->setPositionFlag(sc<IElement::ePositionFlag>(flag), boolean(state, 3));
    return 0;
}

static int elementSetAbsolutePosition(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->setAbsolutePosition(vector(state, 2));
    return 0;
}

static int elementAddChild(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->addChild(checkElement(state, 2));
    return 0;
}

static int elementRemoveChild(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->removeChild(checkElement(state, 2));
    return 0;
}

static int elementClearChildren(lua_State* state) {
    expectArguments(state, 1, 1);
    checkElement(state, 1)->clearChildren();
    return 0;
}

static int elementSetMargin(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->setMargin(floatNumber(state, 2));
    return 0;
}

static int elementSetGrouped(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->setGrouped(boolean(state, 2));
    return 0;
}

static int elementSetReceivesMouse(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->setReceivesMouse(boolean(state, 2));
    return 0;
}

static int elementSetMouseEnter(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto cb      = callback(state, 2, true);
    if (!cb) {
        element->setMouseEnter({});
        return 0;
    }

    element->setMouseEnter([cb](const Hyprutils::Math::Vector2D& position) {
        if (!cb->beginCall())
            return;
        pushVector(cb->state(), position);
        cb->finishCall(1);
    });
    return 0;
}

static int elementSetMouseLeave(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto cb      = callback(state, 2, true);
    if (!cb) {
        element->setMouseLeave({});
        return 0;
    }

    element->setMouseLeave([cb] {
        if (!cb->beginCall())
            return;
        cb->finishCall(0);
    });
    return 0;
}

static int elementSetMouseMove(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto cb      = callback(state, 2, true);
    if (!cb) {
        element->setMouseMove({});
        return 0;
    }

    element->setMouseMove([cb](const Hyprutils::Math::Vector2D& position) {
        if (!cb->beginCall())
            return;
        pushVector(cb->state(), position);
        cb->finishCall(1);
    });
    return 0;
}

static int elementSetMouseButton(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto cb      = callback(state, 2, true);
    if (!cb) {
        element->setMouseButton({});
        return 0;
    }

    element->setMouseButton([cb](Input::eMouseButton button, bool down) {
        if (!cb->beginCall())
            return;
        lua_pushinteger(cb->state(), button);
        lua_pushboolean(cb->state(), down);
        cb->finishCall(2);
    });
    return 0;
}

static int elementSetMouseAxis(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto cb      = callback(state, 2, true);
    if (!cb) {
        element->setMouseAxis({});
        return 0;
    }

    element->setMouseAxis([cb](Input::eAxisAxis axis, float amount) {
        if (!cb->beginCall())
            return;
        lua_pushinteger(cb->state(), axis);
        lua_pushnumber(cb->state(), amount);
        cb->finishCall(2);
    });
    return 0;
}

static int elementSetReceivesTouch(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->setReceivesTouch(boolean(state, 2));
    return 0;
}

template <void (IElement::*Setter)(std::function<void(const Input::STouchEvent&)>&&)>
static int elementSetTouchCallback(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto cb      = callback(state, 2, true);
    if (!cb) {
        (element.get()->*Setter)({});
        return 0;
    }

    (element.get()->*Setter)([cb](const Input::STouchEvent& event) {
        if (!cb->beginCall())
            return;
        pushTouchEvent(cb->state(), event);
        cb->finishCall(1);
    });
    return 0;
}

static int elementSetTooltip(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->setTooltip(string(state, 2));
    return 0;
}

static int elementSetRepositioned(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto cb      = callback(state, 2, true);
    if (!cb) {
        element->setRepositioned({});
        return 0;
    }

    element->setRepositioned([cb] {
        if (!cb->beginCall())
            return;
        cb->finishCall(0);
    });
    return 0;
}

static int elementSetGrow(lua_State* state) {
    expectArguments(state, 2, 3);
    const auto element = checkElement(state, 1);
    if (lua_gettop(state) == 2)
        element->setGrow(boolean(state, 2));
    else
        element->setGrow(boolean(state, 2), boolean(state, 3));
    return 0;
}

static int elementAnimateOpacity(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->animateOpacity(animation(state, 2));
    return 0;
}

static int elementAnimateGeometry(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->animateGeometry(animation(state, 2));
    return 0;
}

static int elementSetOpacity(lua_State* state) {
    expectArguments(state, 2, 2);
    checkElement(state, 1)->setOpacity(floatNumber(state, 2));
    return 0;
}

static int elementForceReposition(lua_State* state) {
    expectArguments(state, 1, 1);
    checkElement(state, 1)->forceReposition();
    return 0;
}

template <typename T>
static bool tryRebuild(lua_State* state, const SP<IElement>& element) {
    const auto concrete = dynamicPointerCast<T>(element);
    if (!concrete)
        return false;
    pushBuilder(state, concrete->rebuild());
    return true;
}

static int elementRebuild(lua_State* state) {
    expectArguments(state, 1, 1);
    const auto element = checkElement(state, 1);
    if (tryRebuild<CTextElement>(state, element) || tryRebuild<CRectangleElement>(state, element) || tryRebuild<CCheckboxElement>(state, element) ||
        tryRebuild<CProgressBarElement>(state, element) || tryRebuild<CScrollAreaElement>(state, element) || tryRebuild<CSpinboxElement>(state, element) ||
        tryRebuild<CSliderElement>(state, element) || tryRebuild<CComboboxElement>(state, element) || tryRebuild<CButtonElement>(state, element) ||
        tryRebuild<CTextboxElement>(state, element) || tryRebuild<CRowLayoutElement>(state, element) || tryRebuild<CImageElement>(state, element) ||
        tryRebuild<CColumnLayoutElement>(state, element) || tryRebuild<CNullElement>(state, element) || tryRebuild<CLineElement>(state, element))
        return 1;
    fail("element does not support rebuild");
}

static int elementSetText(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto text    = string(state, 2);
    if (const auto concrete = dynamicPointerCast<CTextElement>(element))
        concrete->setText(text);
    else if (const auto concrete = dynamicPointerCast<CTextboxElement>(element))
        concrete->setText(text);
    else
        fail("setText is only available on Text and Textbox elements");
    return 0;
}

static int elementAnimateColor(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto value   = animation(state, 2);
    if (const auto concrete = dynamicPointerCast<CTextElement>(element))
        concrete->animateColor(value);
    else if (const auto concrete = dynamicPointerCast<CRectangleElement>(element))
        concrete->animateColor(value);
    else
        fail("animateColor is only available on Text and Rectangle elements");
    return 0;
}

static int rectangleAnimateBorderColor(lua_State* state) {
    expectArguments(state, 2, 2);
    concreteElement<CRectangleElement>(state, 1, "Rectangle")->animateBorderColor(animation(state, 2));
    return 0;
}

static int buttonSetLabel(lua_State* state) {
    expectArguments(state, 2, 2);
    concreteElement<CButtonElement>(state, 1, "Button")->setLabel(string(state, 2));
    return 0;
}

static int buttonSetEnabled(lua_State* state) {
    expectArguments(state, 2, 2);
    concreteElement<CButtonElement>(state, 1, "Button")->setEnabled(boolean(state, 2));
    return 0;
}

static int checkboxState(lua_State* state) {
    expectArguments(state, 1, 1);
    lua_pushboolean(state, concreteElement<CCheckboxElement>(state, 1, "Checkbox")->state());
    return 1;
}

static int checkboxSetState(lua_State* state) {
    expectArguments(state, 2, 2);
    concreteElement<CCheckboxElement>(state, 1, "Checkbox")->setState(boolean(state, 2));
    return 0;
}

static int progressBarValue(lua_State* state) {
    expectArguments(state, 1, 1);
    lua_pushnumber(state, concreteElement<CProgressBarElement>(state, 1, "ProgressBar")->value());
    return 1;
}

static int progressBarSetValue(lua_State* state) {
    expectArguments(state, 2, 2);
    concreteElement<CProgressBarElement>(state, 1, "ProgressBar")->setValue(floatNumber(state, 2));
    return 0;
}

static int scrollAreaGetCurrentScroll(lua_State* state) {
    expectArguments(state, 1, 1);
    pushVector(state, concreteElement<CScrollAreaElement>(state, 1, "ScrollArea")->getCurrentScroll());
    return 1;
}

static int scrollAreaSetScroll(lua_State* state) {
    expectArguments(state, 2, 2);
    concreteElement<CScrollAreaElement>(state, 1, "ScrollArea")->setScroll(vector(state, 2));
    return 0;
}

static int indexedElementCurrent(lua_State* state) {
    expectArguments(state, 1, 1);
    const auto element = checkElement(state, 1);
    if (const auto concrete = dynamicPointerCast<CSpinboxElement>(element))
        pushIndex(state, concrete->current());
    else if (const auto concrete = dynamicPointerCast<CComboboxElement>(element))
        pushIndex(state, concrete->current());
    else
        fail("current is only available on Spinbox and Combobox elements");
    return 1;
}

static int indexedElementSetCurrent(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto element = checkElement(state, 1);
    const auto value   = luaIndex(state, 2);
    if (const auto concrete = dynamicPointerCast<CSpinboxElement>(element))
        concrete->setCurrent(value);
    else if (const auto concrete = dynamicPointerCast<CComboboxElement>(element))
        concrete->setCurrent(value);
    else
        fail("setCurrent is only available on Spinbox and Combobox elements");
    return 0;
}

static int sliderSliding(lua_State* state) {
    expectArguments(state, 1, 1);
    lua_pushboolean(state, concreteElement<CSliderElement>(state, 1, "Slider")->sliding());
    return 1;
}

static int textboxFocus(lua_State* state) {
    expectArguments(state, 1, 2);
    concreteElement<CTextboxElement>(state, 1, "Textbox")->focus(lua_gettop(state) == 1 ? true : boolean(state, 2));
    return 0;
}

static int textboxCurrentText(lua_State* state) {
    expectArguments(state, 1, 1);
    const auto text = concreteElement<CTextboxElement>(state, 1, "Textbox")->currentText();
    lua_pushlstring(state, text.data(), text.size());
    return 1;
}

static int textboxCursorPos(lua_State* state) {
    expectArguments(state, 1, 1);
    const auto position = concreteElement<CTextboxElement>(state, 1, "Textbox")->cursorPos();
    if (position > sc<size_t>(std::numeric_limits<lua_Integer>::max()))
        fail("cursor position is too large for Lua");
    lua_pushinteger(state, sc<lua_Integer>(position));
    return 1;
}

static int textboxSelection(lua_State* state) {
    expectArguments(state, 1, 1);
    const auto [begin, end] = concreteElement<CTextboxElement>(state, 1, "Textbox")->selection();
    lua_pushinteger(state, sc<lua_Integer>(begin));
    lua_pushinteger(state, sc<lua_Integer>(end));
    return 2;
}

static int textboxSetPassword(lua_State* state) {
    expectArguments(state, 2, 2);
    concreteElement<CTextboxElement>(state, 1, "Textbox")->setPassword(boolean(state, 2));
    return 0;
}

static SP<CRadioGroup> checkRadioGroup(lua_State* state, int index) {
    const auto group = object<SP<CRadioGroup>>(state, index, MT_RADIO_GROUP);
    if (!group)
        fail("argument " + std::to_string(index) + " is an empty RadioGroup");
    return group;
}

static int radioGroupCreate(lua_State* state) {
    expectArguments(state, 0, 0);
    pushObject(state, CRadioGroup::create(), MT_RADIO_GROUP);
    return 1;
}

static int radioGroupAdd(lua_State* state) {
    expectArguments(state, 2, 2);
    checkRadioGroup(state, 1)->add(concreteElement<CCheckboxElement>(state, 2, "Checkbox"));
    return 0;
}

static int radioGroupSelected(lua_State* state) {
    expectArguments(state, 1, 1);
    pushElement(state, checkRadioGroup(state, 1)->selected());
    return 1;
}

static int radioGroupSetSelected(lua_State* state) {
    expectArguments(state, 2, 2);
    if (lua_isnil(state, 2)) {
        checkRadioGroup(state, 1)->setSelected({});
        return 0;
    }
    checkRadioGroup(state, 1)->setSelected(concreteElement<CCheckboxElement>(state, 2, "Checkbox"));
    return 0;
}

static int radioGroupOnSelected(lua_State* state) {
    expectArguments(state, 2, 2);
    const auto group = checkRadioGroup(state, 1);
    const auto cb    = callback(state, 2, true);
    if (!cb) {
        group->onSelected({});
        return 0;
    }

    group->onSelected([cb](SP<CCheckboxElement> selected) {
        if (!cb->beginCall())
            return;
        pushElement(cb->state(), selected);
        cb->finishCall(1);
    });
    return 0;
}

static constexpr luaL_Reg ELEMENT_METHODS[] = {
    {"size", protect<elementSize>},
    {"posFromParent", protect<elementPosFromParent>},
    {"setPositionMode", protect<elementSetPositionMode>},
    {"setPositionFlag", protect<elementSetPositionFlag>},
    {"setAbsolutePosition", protect<elementSetAbsolutePosition>},
    {"addChild", protect<elementAddChild>},
    {"removeChild", protect<elementRemoveChild>},
    {"clearChildren", protect<elementClearChildren>},
    {"setMargin", protect<elementSetMargin>},
    {"setGrouped", protect<elementSetGrouped>},
    {"setReceivesMouse", protect<elementSetReceivesMouse>},
    {"setMouseEnter", protect<elementSetMouseEnter>},
    {"setMouseLeave", protect<elementSetMouseLeave>},
    {"setMouseMove", protect<elementSetMouseMove>},
    {"setMouseButton", protect<elementSetMouseButton>},
    {"setMouseAxis", protect<elementSetMouseAxis>},
    {"setReceivesTouch", protect<elementSetReceivesTouch>},
    {"setTouchDown", protect<elementSetTouchCallback<&IElement::setTouchDown>>},
    {"setTouchMotion", protect<elementSetTouchCallback<&IElement::setTouchMotion>>},
    {"setTouchUp", protect<elementSetTouchCallback<&IElement::setTouchUp>>},
    {"setTouchCancel", protect<elementSetTouchCallback<&IElement::setTouchCancel>>},
    {"setTooltip", protect<elementSetTooltip>},
    {"setRepositioned", protect<elementSetRepositioned>},
    {"setGrow", protect<elementSetGrow>},
    {"animateOpacity", protect<elementAnimateOpacity>},
    {"animateGeometry", protect<elementAnimateGeometry>},
    {"setOpacity", protect<elementSetOpacity>},
    {"forceReposition", protect<elementForceReposition>},
    {"rebuild", protect<elementRebuild>},
    {"setText", protect<elementSetText>},
    {"animateColor", protect<elementAnimateColor>},
    {"animateBorderColor", protect<rectangleAnimateBorderColor>},
    {"setLabel", protect<buttonSetLabel>},
    {"setEnabled", protect<buttonSetEnabled>},
    {"state", protect<checkboxState>},
    {"setState", protect<checkboxSetState>},
    {"value", protect<progressBarValue>},
    {"setValue", protect<progressBarSetValue>},
    {"getCurrentScroll", protect<scrollAreaGetCurrentScroll>},
    {"setScroll", protect<scrollAreaSetScroll>},
    {"current", protect<indexedElementCurrent>},
    {"setCurrent", protect<indexedElementSetCurrent>},
    {"sliding", protect<sliderSliding>},
    {"focus", protect<textboxFocus>},
    {"currentText", protect<textboxCurrentText>},
    {"cursorPos", protect<textboxCursorPos>},
    {"selection", protect<textboxSelection>},
    {"setPassword", protect<textboxSetPassword>},
    {nullptr, nullptr},
};

static constexpr luaL_Reg RADIO_GROUP_METHODS[] = {
    {"add", protect<radioGroupAdd>},
    {"selected", protect<radioGroupSelected>},
    {"setSelected", protect<radioGroupSetSelected>},
    {"onSelected", protect<radioGroupOnSelected>},
    {nullptr, nullptr},
};

static constexpr luaL_Reg RADIO_GROUP_CLASS[] = {
    {"create", protect<radioGroupCreate>},
    {nullptr, nullptr},
};

void Hyprtoolkit::Lua::registerElements(lua_State* state, int module) {
    registerMetatable(state, MT_ELEMENT, ELEMENT_METHODS, protect<destroyObject<SP<IElement>>>);
    registerMetatable(state, MT_RADIO_GROUP, RADIO_GROUP_METHODS, protect<destroyObject<SP<CRadioGroup>>>);
    setClass(state, module, "RadioGroup", RADIO_GROUP_CLASS);
}
