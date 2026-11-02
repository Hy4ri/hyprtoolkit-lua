#include "../Binding.hpp"

#include <format>

using namespace Hyprtoolkit;
using namespace Hyprtoolkit::Lua;
using namespace Hyprutils::Memory;

static constexpr std::array BUILDER_NAMES = {
    "TextBuilder",   "RectangleBuilder", "CheckboxBuilder",  "ProgressBarBuilder", "ScrollAreaBuilder",   "SpinboxBuilder", "SliderBuilder", "ComboboxBuilder",
    "ButtonBuilder", "TextboxBuilder",   "RowLayoutBuilder", "ImageBuilder",       "ColumnLayoutBuilder", "NullBuilder",    "LineBuilder",
};

static SBuilderVariant& builderVariant(lua_State* state) {
    return object<SBuilderVariant>(state, 1, MT_BUILDER);
}

[[noreturn]] static void invalidMethod(const SBuilderVariant& builder, const char* method) {
    const auto index = builder.index();
    if (index >= BUILDER_NAMES.size())
        fail("method '" + std::string{method} + "' is not available on an invalid builder");
    fail("method '" + std::string{method} + "' is not available on " + BUILDER_NAMES.at(index));
}

template <typename... Builders, typename Apply>
static int applyTo(lua_State* state, const char* method, Apply&& apply) {
    expectArguments(state, 2, 2);
    auto&      builder = builderVariant(state);

    const bool applied = ([&] {
        const auto concrete = std::get_if<SP<Builders>>(&builder);
        if (!concrete)
            return false;
        apply(*concrete);
        return true;
    }() || ...);

    if (!applied)
        invalidMethod(builder, method);

    lua_pushvalue(state, 1);
    return 1;
}

static eFontAlignment fontAlignment(lua_State* state, int index) {
    const auto value = integer(state, index);
    if (value < HT_FONT_ALIGN_LEFT || value > HT_FONT_ALIGN_RIGHT)
        fail("argument " + std::to_string(index) + " is not a valid font alignment");
    return sc<eFontAlignment>(value);
}

static eCheckboxStyle checkboxStyle(lua_State* state, int index) {
    const auto value = integer(state, index);
    if (value < HT_CHECKBOX_STYLE_CHECKMARK || value > HT_CHECKBOX_STYLE_RADIO)
        fail("argument " + std::to_string(index) + " is not a valid checkbox style");
    return sc<eCheckboxStyle>(value);
}

static eImageFitMode imageFitMode(lua_State* state, int index) {
    const auto value = integer(state, index);
    if (value < IMAGE_FIT_MODE_STRETCH || value > IMAGE_FIT_MODE_TILE)
        fail("argument " + std::to_string(index) + " is not a valid image fit mode");
    return sc<eImageFitMode>(value);
}

static size_t nonNegativeSize(lua_State* state, int index, const char* name) {
    const auto value = integer(state, index);
    if (value < 0)
        fail(std::string{name} + " must be non-negative");
    return sc<size_t>(value);
}

static size_t itemIndex(lua_State* state, int index) {
    const auto value = integer(state, index);
    if (value < 1)
        fail("argument " + std::to_string(index) + " must be an integer greater than or equal to 1");
    return sc<size_t>(value - 1);
}

static colorFn colorFunction(lua_State* state, int index) {
    if (!lua_isfunction(state, index)) {
        const auto value = color(state, index);
        return [value] { return value; };
    }

    const auto luaCallback = callback(state, index);
    return [source = luaCallback] {
        const auto luaCallback = source;
        if (!luaCallback->beginCall())
            return CHyprColor{};
        auto*     state = luaCallback->state();
        const int top   = lua_gettop(state) - 1;
        if (!luaCallback->finishCall(0, 1))
            return CHyprColor{};

        try {
            const auto result = color(state, -1);
            lua_settop(state, top);
            return result;
        } catch (const std::exception& error) {
            lua_settop(state, top);
            luaCallback->reportError(std::format("color callback: {}", error.what()));
            return CHyprColor{};
        }
    };
}

static gradientFn gradientFunction(lua_State* state, int index) {
    if (!lua_isfunction(state, index)) {
        const auto value = gradient(state, index);
        return [value] { return value; };
    }

    const auto luaCallback = callback(state, index);
    return [source = luaCallback] {
        const auto luaCallback = source;
        if (!luaCallback->beginCall())
            return CGradientValueData{CHyprColor{}};
        auto*     state = luaCallback->state();
        const int top   = lua_gettop(state) - 1;
        if (!luaCallback->finishCall(0, 1))
            return CGradientValueData{CHyprColor{}};

        try {
            const auto result = gradient(state, -1);
            lua_settop(state, top);
            return result;
        } catch (const std::exception& error) {
            lua_settop(state, top);
            luaCallback->reportError(std::format("gradient callback: {}", error.what()));
            return CGradientValueData{CHyprColor{}};
        }
    };
}

template <typename Element>
static std::function<void(SP<Element>)> elementCallback(lua_State* state, int index) {
    const auto luaCallback = callback(state, index, true);
    if (!luaCallback)
        return {};

    return [luaCallback](SP<Element> element) {
        if (!luaCallback->beginCall())
            return;
        pushElement(luaCallback->state(), element);
        luaCallback->finishCall(1);
    };
}

template <typename Element>
static std::function<void(SP<Element>, size_t)> indexedElementCallback(lua_State* state, int index) {
    const auto luaCallback = callback(state, index, true);
    if (!luaCallback)
        return {};

    return [luaCallback](SP<Element> element, size_t item) {
        if (!luaCallback->beginCall())
            return;
        pushElement(luaCallback->state(), element);
        lua_pushinteger(luaCallback->state(), sc<lua_Integer>(item) + 1);
        luaCallback->finishCall(2);
    };
}

static int setSize(lua_State* state) {
    return applyTo<CTextBuilder, CRectangleBuilder, CCheckboxBuilder, CProgressBarBuilder, CScrollAreaBuilder, CSpinboxBuilder, CSliderBuilder, CComboboxBuilder, CButtonBuilder,
                   CTextboxBuilder, CRowLayoutBuilder, CImageBuilder, CColumnLayoutBuilder, CNullBuilder, CLineBuilder>(
        state, "size", [&](auto& builder) { builder->size(dynamicSize(state, 2)); });
}

static int setColor(lua_State* state) {
    return applyTo<CTextBuilder, CRectangleBuilder, CLineBuilder>(state, "color", [&](auto& builder) { builder->color(colorFunction(state, 2)); });
}

static int setAlpha(lua_State* state) {
    return applyTo<CTextBuilder, CImageBuilder>(state, "a", [&](auto& builder) { builder->a(floatNumber(state, 2)); });
}

static int setFontSize(lua_State* state) {
    return applyTo<CTextBuilder, CButtonBuilder>(state, "fontSize", [&](auto& builder) { builder->fontSize(fontSize(state, 2)); });
}

static int setFontFamily(lua_State* state) {
    return applyTo<CTextBuilder, CButtonBuilder>(state, "fontFamily", [&](auto& builder) { builder->fontFamily(string(state, 2)); });
}

static int setTextAlign(lua_State* state) {
    return applyTo<CTextBuilder>(state, "align", [&](auto& builder) { builder->align(fontAlignment(state, 2)); });
}

static int setText(lua_State* state) {
    return applyTo<CTextBuilder>(state, "text", [&](auto& builder) { builder->text(string(state, 2)); });
}

static int setClampSize(lua_State* state) {
    return applyTo<CTextBuilder>(state, "clampSize", [&](auto& builder) { builder->clampSize(vector(state, 2)); });
}

static int setTextCallback(lua_State* state) {
    return applyTo<CTextBuilder>(state, "callback", [&](auto& builder) {
        const auto            luaCallback = callback(state, 2, true);
        std::function<void()> function;
        if (luaCallback)
            function = [luaCallback] {
                if (!luaCallback->beginCall())
                    return;
                luaCallback->finishCall(0);
            };
        builder->callback(std::move(function));
    });
}

static int setNoEllipsize(lua_State* state) {
    return applyTo<CTextBuilder>(state, "noEllipsize", [&](auto& builder) { builder->noEllipsize(boolean(state, 2)); });
}

static int setAsync(lua_State* state) {
    return applyTo<CTextBuilder>(state, "async", [&](auto& builder) { builder->async(boolean(state, 2)); });
}

static int setInteractable(lua_State* state) {
    return applyTo<CTextBuilder>(state, "interactable", [&](auto& builder) { builder->interactable(boolean(state, 2)); });
}

static int setBorderColor(lua_State* state) {
    return applyTo<CRectangleBuilder>(state, "borderColor", [&](auto& builder) { builder->borderColor(colorFunction(state, 2)); });
}

static int setBorderGradient(lua_State* state) {
    return applyTo<CRectangleBuilder>(state, "borderGradient", [&](auto& builder) { builder->borderGradient(gradientFunction(state, 2)); });
}

static int setRounding(lua_State* state) {
    return applyTo<CRectangleBuilder, CImageBuilder>(state, "rounding", [&](auto& builder) { builder->rounding(intNumber(state, 2)); });
}

static int setBorderThickness(lua_State* state) {
    return applyTo<CRectangleBuilder>(state, "borderThickness", [&](auto& builder) { builder->borderThickness(intNumber(state, 2)); });
}

static int setOnToggled(lua_State* state) {
    return applyTo<CCheckboxBuilder>(state, "onToggled", [&](auto& builder) {
        const auto                                      luaCallback = callback(state, 2, true);
        std::function<void(SP<CCheckboxElement>, bool)> function;
        if (luaCallback)
            function = [luaCallback](SP<CCheckboxElement> element, bool toggled) {
                if (!luaCallback->beginCall())
                    return;
                pushElement(luaCallback->state(), element);
                lua_pushboolean(luaCallback->state(), toggled);
                luaCallback->finishCall(2);
            };
        builder->onToggled(std::move(function));
    });
}

static int setToggled(lua_State* state) {
    return applyTo<CCheckboxBuilder>(state, "toggled", [&](auto& builder) { builder->toggled(boolean(state, 2)); });
}

static int setCheckboxStyle(lua_State* state) {
    return applyTo<CCheckboxBuilder>(state, "style", [&](auto& builder) { builder->style(checkboxStyle(state, 2)); });
}

static int setProgressValue(lua_State* state) {
    return applyTo<CProgressBarBuilder>(state, "value", [&](auto& builder) { builder->value(floatNumber(state, 2)); });
}

static int setIndeterminate(lua_State* state) {
    return applyTo<CProgressBarBuilder>(state, "indeterminate", [&](auto& builder) { builder->indeterminate(boolean(state, 2)); });
}

static int setScrollX(lua_State* state) {
    return applyTo<CScrollAreaBuilder>(state, "scrollX", [&](auto& builder) { builder->scrollX(boolean(state, 2)); });
}

static int setScrollY(lua_State* state) {
    return applyTo<CScrollAreaBuilder>(state, "scrollY", [&](auto& builder) { builder->scrollY(boolean(state, 2)); });
}

static int setBlockUserScroll(lua_State* state) {
    return applyTo<CScrollAreaBuilder>(state, "blockUserScroll", [&](auto& builder) { builder->blockUserScroll(boolean(state, 2)); });
}

static int setShowScrollbar(lua_State* state) {
    return applyTo<CScrollAreaBuilder>(state, "showScrollbar", [&](auto& builder) { builder->showScrollbar(boolean(state, 2)); });
}

static int setLabel(lua_State* state) {
    return applyTo<CSpinboxBuilder, CButtonBuilder>(state, "label", [&](auto& builder) { builder->label(string(state, 2)); });
}

static int setItems(lua_State* state) {
    return applyTo<CSpinboxBuilder, CComboboxBuilder>(state, "items", [&](auto& builder) { builder->items(stringArray(state, 2)); });
}

static int setCurrentItem(lua_State* state) {
    return applyTo<CSpinboxBuilder, CComboboxBuilder>(state, "currentItem", [&](auto& builder) { builder->currentItem(itemIndex(state, 2)); });
}

static int setOnChanged(lua_State* state) {
    expectArguments(state, 2, 2);
    auto& builder = builderVariant(state);

    if (const auto spinbox = std::get_if<SP<CSpinboxBuilder>>(&builder))
        (*spinbox)->onChanged(indexedElementCallback<CSpinboxElement>(state, 2));
    else if (const auto slider = std::get_if<SP<CSliderBuilder>>(&builder)) {
        const auto                                     luaCallback = callback(state, 2, true);
        std::function<void(SP<CSliderElement>, float)> function;
        if (luaCallback)
            function = [luaCallback](SP<CSliderElement> element, float value) {
                if (!luaCallback->beginCall())
                    return;
                pushElement(luaCallback->state(), element);
                lua_pushnumber(luaCallback->state(), value);
                luaCallback->finishCall(2);
            };
        (*slider)->onChanged(std::move(function));
    } else if (const auto combobox = std::get_if<SP<CComboboxBuilder>>(&builder))
        (*combobox)->onChanged(indexedElementCallback<CComboboxElement>(state, 2));
    else
        invalidMethod(builder, "onChanged");

    lua_pushvalue(state, 1);
    return 1;
}

static int setFill(lua_State* state) {
    return applyTo<CSpinboxBuilder>(state, "fill", [&](auto& builder) { builder->fill(boolean(state, 2)); });
}

static int setSliderMin(lua_State* state) {
    return applyTo<CSliderBuilder>(state, "min", [&](auto& builder) { builder->min(floatNumber(state, 2)); });
}

static int setSliderMax(lua_State* state) {
    return applyTo<CSliderBuilder>(state, "max", [&](auto& builder) { builder->max(floatNumber(state, 2)); });
}

static int setSliderValue(lua_State* state) {
    return applyTo<CSliderBuilder>(state, "val", [&](auto& builder) { builder->val(floatNumber(state, 2)); });
}

static int setSnapInt(lua_State* state) {
    return applyTo<CSliderBuilder>(state, "snapInt", [&](auto& builder) { builder->snapInt(boolean(state, 2)); });
}

static int setNoBorder(lua_State* state) {
    return applyTo<CButtonBuilder>(state, "noBorder", [&](auto& builder) { builder->noBorder(boolean(state, 2)); });
}

static int setNoBg(lua_State* state) {
    return applyTo<CButtonBuilder>(state, "noBg", [&](auto& builder) { builder->noBg(boolean(state, 2)); });
}

static int setAccent(lua_State* state) {
    return applyTo<CButtonBuilder>(state, "accent", [&](auto& builder) { builder->accent(boolean(state, 2)); });
}

static int setEllipsize(lua_State* state) {
    return applyTo<CButtonBuilder>(state, "ellipsize", [&](auto& builder) { builder->ellipsize(boolean(state, 2)); });
}

static int setEnabled(lua_State* state) {
    return applyTo<CButtonBuilder>(state, "enabled", [&](auto& builder) { builder->enabled(boolean(state, 2)); });
}

static int setAlignText(lua_State* state) {
    return applyTo<CButtonBuilder>(state, "alignText", [&](auto& builder) { builder->alignText(fontAlignment(state, 2)); });
}

static int setOnMainClick(lua_State* state) {
    return applyTo<CButtonBuilder>(state, "onMainClick", [&](auto& builder) { builder->onMainClick(elementCallback<CButtonElement>(state, 2)); });
}

static int setOnRightClick(lua_State* state) {
    return applyTo<CButtonBuilder>(state, "onRightClick", [&](auto& builder) { builder->onRightClick(elementCallback<CButtonElement>(state, 2)); });
}

static int setPlaceholder(lua_State* state) {
    return applyTo<CTextboxBuilder>(state, "placeholder", [&](auto& builder) { builder->placeholder(string(state, 2)); });
}

static int setDefaultText(lua_State* state) {
    return applyTo<CTextboxBuilder>(state, "defaultText", [&](auto& builder) { builder->defaultText(string(state, 2)); });
}

static int setOnTextEdited(lua_State* state) {
    return applyTo<CTextboxBuilder>(state, "onTextEdited", [&](auto& builder) {
        const auto                                                   luaCallback = callback(state, 2, true);
        std::function<void(SP<CTextboxElement>, const std::string&)> function;
        if (luaCallback)
            function = [luaCallback](SP<CTextboxElement> element, const std::string& text) {
                if (!luaCallback->beginCall())
                    return;
                pushElement(luaCallback->state(), element);
                lua_pushlstring(luaCallback->state(), text.data(), text.size());
                luaCallback->finishCall(2);
            };
        builder->onTextEdited(std::move(function));
    });
}

static int setMultiline(lua_State* state) {
    return applyTo<CTextboxBuilder>(state, "multiline", [&](auto& builder) { builder->multiline(boolean(state, 2)); });
}

static int setPassword(lua_State* state) {
    return applyTo<CTextboxBuilder>(state, "password", [&](auto& builder) { builder->password(boolean(state, 2)); });
}

static int setEyeIcon(lua_State* state) {
    return applyTo<CTextboxBuilder>(state, "eyeIcon", [&](auto& builder) { builder->eyeIcon(boolean(state, 2)); });
}

static int setGap(lua_State* state) {
    return applyTo<CRowLayoutBuilder, CColumnLayoutBuilder>(state, "gap", [&](auto& builder) { builder->gap(nonNegativeSize(state, 2, "gap")); });
}

static int setImagePath(lua_State* state) {
    return applyTo<CImageBuilder>(state, "path", [&](auto& builder) { builder->path(string(state, 2)); });
}

static int setImageIcon(lua_State* state) {
    return applyTo<CImageBuilder>(state, "icon", [&](auto& builder) { builder->icon(object<SP<ISystemIconDescription>>(state, 2, MT_ICON)); });
}

static int setImageData(lua_State* state) {
    return applyTo<CImageBuilder>(state, "data", [&](auto& builder) {
        const auto bytes = string(state, 2);
        builder->data(std::vector<uint8_t>{bytes.begin(), bytes.end()});
    });
}

static int setFitMode(lua_State* state) {
    return applyTo<CImageBuilder>(state, "fitMode", [&](auto& builder) { builder->fitMode(imageFitMode(state, 2)); });
}

static int setSync(lua_State* state) {
    return applyTo<CImageBuilder>(state, "sync", [&](auto& builder) { builder->sync(boolean(state, 2)); });
}

static int setLineThickness(lua_State* state) {
    return applyTo<CLineBuilder>(state, "thick", [&](auto& builder) { builder->thick(intNumber(state, 2)); });
}

static int setLinePoints(lua_State* state) {
    return applyTo<CLineBuilder>(state, "points", [&](auto& builder) { builder->points(vectorArray(state, 2)); });
}

static int commence(lua_State* state) {
    expectArguments(state, 1, 1);
    requireBackendActive(state);
    auto& builder = builderVariant(state);
    std::visit([&](auto& concrete) { pushElement(state, concrete->commence()); }, builder);
    return 1;
}

template <typename Builder>
static int beginBuilder(lua_State* state) {
    expectArguments(state, 0, 0);
    pushBuilder(state, Builder::begin());
    return 1;
}

static constexpr luaL_Reg BUILDER_METHODS[] = {
    {"size", protect<setSize>},
    {"color", protect<setColor>},
    {"a", protect<setAlpha>},
    {"fontSize", protect<setFontSize>},
    {"fontFamily", protect<setFontFamily>},
    {"align", protect<setTextAlign>},
    {"text", protect<setText>},
    {"clampSize", protect<setClampSize>},
    {"callback", protect<setTextCallback>},
    {"noEllipsize", protect<setNoEllipsize>},
    {"async", protect<setAsync>},
    {"interactable", protect<setInteractable>},
    {"borderColor", protect<setBorderColor>},
    {"borderGradient", protect<setBorderGradient>},
    {"rounding", protect<setRounding>},
    {"borderThickness", protect<setBorderThickness>},
    {"onToggled", protect<setOnToggled>},
    {"toggled", protect<setToggled>},
    {"style", protect<setCheckboxStyle>},
    {"value", protect<setProgressValue>},
    {"indeterminate", protect<setIndeterminate>},
    {"scrollX", protect<setScrollX>},
    {"scrollY", protect<setScrollY>},
    {"blockUserScroll", protect<setBlockUserScroll>},
    {"showScrollbar", protect<setShowScrollbar>},
    {"label", protect<setLabel>},
    {"items", protect<setItems>},
    {"currentItem", protect<setCurrentItem>},
    {"onChanged", protect<setOnChanged>},
    {"fill", protect<setFill>},
    {"min", protect<setSliderMin>},
    {"max", protect<setSliderMax>},
    {"val", protect<setSliderValue>},
    {"snapInt", protect<setSnapInt>},
    {"noBorder", protect<setNoBorder>},
    {"noBg", protect<setNoBg>},
    {"accent", protect<setAccent>},
    {"ellipsize", protect<setEllipsize>},
    {"enabled", protect<setEnabled>},
    {"alignText", protect<setAlignText>},
    {"onMainClick", protect<setOnMainClick>},
    {"onRightClick", protect<setOnRightClick>},
    {"placeholder", protect<setPlaceholder>},
    {"defaultText", protect<setDefaultText>},
    {"onTextEdited", protect<setOnTextEdited>},
    {"multiline", protect<setMultiline>},
    {"password", protect<setPassword>},
    {"eyeIcon", protect<setEyeIcon>},
    {"gap", protect<setGap>},
    {"path", protect<setImagePath>},
    {"icon", protect<setImageIcon>},
    {"data", protect<setImageData>},
    {"fitMode", protect<setFitMode>},
    {"sync", protect<setSync>},
    {"thick", protect<setLineThickness>},
    {"points", protect<setLinePoints>},
    {"commence", protect<commence>},
    {nullptr, nullptr},
};

#define BUILDER_CLASS(NAME, TYPE)                                                                                                                                                  \
    static constexpr luaL_Reg NAME[] = {                                                                                                                                           \
        {"begin", protect<beginBuilder<TYPE>>},                                                                                                                                    \
        {nullptr, nullptr},                                                                                                                                                        \
    }

BUILDER_CLASS(TEXT_BUILDER_CLASS, CTextBuilder);
BUILDER_CLASS(RECTANGLE_BUILDER_CLASS, CRectangleBuilder);
BUILDER_CLASS(CHECKBOX_BUILDER_CLASS, CCheckboxBuilder);
BUILDER_CLASS(PROGRESS_BAR_BUILDER_CLASS, CProgressBarBuilder);
BUILDER_CLASS(SCROLL_AREA_BUILDER_CLASS, CScrollAreaBuilder);
BUILDER_CLASS(SPINBOX_BUILDER_CLASS, CSpinboxBuilder);
BUILDER_CLASS(SLIDER_BUILDER_CLASS, CSliderBuilder);
BUILDER_CLASS(COMBOBOX_BUILDER_CLASS, CComboboxBuilder);
BUILDER_CLASS(BUTTON_BUILDER_CLASS, CButtonBuilder);
BUILDER_CLASS(TEXTBOX_BUILDER_CLASS, CTextboxBuilder);
BUILDER_CLASS(ROW_LAYOUT_BUILDER_CLASS, CRowLayoutBuilder);
BUILDER_CLASS(IMAGE_BUILDER_CLASS, CImageBuilder);
BUILDER_CLASS(COLUMN_LAYOUT_BUILDER_CLASS, CColumnLayoutBuilder);
BUILDER_CLASS(NULL_BUILDER_CLASS, CNullBuilder);
BUILDER_CLASS(LINE_BUILDER_CLASS, CLineBuilder);

#undef BUILDER_CLASS

void Hyprtoolkit::Lua::pushBuilder(lua_State* state, SBuilderVariant builder) {
    pushObject(state, std::move(builder), MT_BUILDER);
}

void Hyprtoolkit::Lua::registerElementBuilders(lua_State* state, int module) {
    module = lua_absindex(state, module);
    registerMetatable(state, MT_BUILDER, BUILDER_METHODS, protect<destroyObject<SBuilderVariant>>);

    setClass(state, module, "TextBuilder", TEXT_BUILDER_CLASS);
    setClass(state, module, "RectangleBuilder", RECTANGLE_BUILDER_CLASS);
    setClass(state, module, "CheckboxBuilder", CHECKBOX_BUILDER_CLASS);
    setClass(state, module, "ProgressBarBuilder", PROGRESS_BAR_BUILDER_CLASS);
    setClass(state, module, "ScrollAreaBuilder", SCROLL_AREA_BUILDER_CLASS);
    setClass(state, module, "SpinboxBuilder", SPINBOX_BUILDER_CLASS);
    setClass(state, module, "SliderBuilder", SLIDER_BUILDER_CLASS);
    setClass(state, module, "ComboboxBuilder", COMBOBOX_BUILDER_CLASS);
    setClass(state, module, "ButtonBuilder", BUTTON_BUILDER_CLASS);
    setClass(state, module, "TextboxBuilder", TEXTBOX_BUILDER_CLASS);
    setClass(state, module, "RowLayoutBuilder", ROW_LAYOUT_BUILDER_CLASS);
    setClass(state, module, "ImageBuilder", IMAGE_BUILDER_CLASS);
    setClass(state, module, "ColumnLayoutBuilder", COLUMN_LAYOUT_BUILDER_CLASS);
    setClass(state, module, "NullBuilder", NULL_BUILDER_CLASS);
    setClass(state, module, "LineBuilder", LINE_BUILDER_CLASS);
}
