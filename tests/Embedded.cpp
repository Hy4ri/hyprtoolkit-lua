#include <hyprtoolkit-lua/Embedding.hpp>
#include <hyprtoolkit-lua/LuaBindings.hpp>
#include <hyprtoolkit/core/EmbeddedBackend.hpp>
#include <hyprtoolkit/core/Timer.hpp>
#include <hyprtoolkit/element/Element.hpp>
#include <hyprtoolkit/window/EmbeddedSurface.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

extern "C" {
#include <lauxlib.h>
}

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <mutex>
#include <stdexcept>

using namespace Hyprtoolkit;
using namespace Hyprtoolkit::Lua;
using namespace Hyprutils::Memory;

template <typename T>
using SP = CSharedPointer<T>;

using TimerPointer = CAtomicSharedPointer<CTimer>;

static void check(bool condition, std::string_view message) {
    if (!condition)
        throw std::runtime_error(std::string{message});
}

class CEGLContext {
  public:
    ~CEGLContext();
    bool init();

  private:
    EGLDisplay m_display = EGL_NO_DISPLAY;
    EGLContext m_context = EGL_NO_CONTEXT;
    EGLSurface m_surface = EGL_NO_SURFACE;
};

bool CEGLContext::init() {
    m_display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    if (m_display == EGL_NO_DISPLAY || !eglInitialize(m_display, nullptr, nullptr) || !eglBindAPI(EGL_OPENGL_ES_API))
        return false;
    constexpr EGLint CONFIG[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE,
    };
    EGLConfig config = nullptr;
    EGLint    count  = 0;
    if (!eglChooseConfig(m_display, CONFIG, &config, 1, &count) || count != 1)
        return false;
    constexpr EGLint CONTEXT[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};
    m_context                  = eglCreateContext(m_display, config, EGL_NO_CONTEXT, CONTEXT);
    if (m_context == EGL_NO_CONTEXT)
        return false;
    constexpr EGLint SURFACE[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
    m_surface                  = eglCreatePbufferSurface(m_display, config, SURFACE);
    return m_surface != EGL_NO_SURFACE && eglMakeCurrent(m_display, m_surface, m_surface, m_context);
}

CEGLContext::~CEGLContext() {
    if (m_display == EGL_NO_DISPLAY)
        return;
    eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (m_surface != EGL_NO_SURFACE)
        eglDestroySurface(m_display, m_surface);
    if (m_context != EGL_NO_CONTEXT)
        eglDestroyContext(m_display, m_context);
    eglTerminate(m_display);
}

class CFramebuffer {
  public:
    ~CFramebuffer();
    void                   init(int size);
    void                   render(const SP<IEmbeddedSurface>& surface);
    std::array<GLubyte, 4> pixel();

  private:
    GLuint m_framebuffer = 0;
    GLuint m_texture     = 0;
    int    m_size        = 0;
};

void CFramebuffer::init(int size) {
    m_size = size;
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &m_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "create host framebuffer");
}

CFramebuffer::~CFramebuffer() {
    glDeleteFramebuffers(1, &m_framebuffer);
    glDeleteTextures(1, &m_texture);
}

void CFramebuffer::render(const SP<IEmbeddedSurface>& surface) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    glViewport(3, 4, 9, 10);
    surface->render(0);
    std::array<GLint, 4> viewport = {};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    check(viewport == std::array<GLint, 4>{3, 4, 9, 10}, "toolkit restores host viewport");
    check(glGetError() == GL_NO_ERROR, "embedded rendering has no GL errors");
}

std::array<GLubyte, 4> CFramebuffer::pixel() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    std::array<GLubyte, 4> pixel = {};
    glReadPixels(m_size / 2, m_size / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    return pixel;
}

class CTestEventLoop : public IEventLoop {
  public:
    void         addFd(int, std::function<void()>&&) override;
    void         removeFd(int) override;
    TimerPointer addTimer(const TimerDuration&, std::function<void(TimerPointer, void*)>, void*, bool) override;
    void         addIdle(const std::function<void()>&) override;
    void         cancelPending() override;
    void         enterLoop() override;
    void         dispatch();

  private:
    std::mutex                         m_mutex;
    std::vector<std::function<void()>> m_idles;
    std::vector<TimerPointer>          m_timers;
};

void CTestEventLoop::addFd(int, std::function<void()>&&) {
    throw std::runtime_error("these embedded widgets must not require native FDs");
}

void CTestEventLoop::removeFd(int) {
    ;
}

TimerPointer CTestEventLoop::addTimer(const TimerDuration& duration, std::function<void(TimerPointer, void*)> callback, void* data, bool force) {
    auto timer = makeAtomicShared<CTimer>(duration, std::move(callback), data, force);
    m_timers.emplace_back(timer);
    return timer;
}

void CTestEventLoop::addIdle(const std::function<void()>& callback) {
    const std::lock_guard lock(m_mutex);
    m_idles.emplace_back(callback);
}

void CTestEventLoop::cancelPending() {
    const std::lock_guard lock(m_mutex);
    m_idles.clear();
    m_timers.clear();
}

void CTestEventLoop::enterLoop() {
    throw std::runtime_error("the host owns the event loop");
}

void CTestEventLoop::dispatch() {
    std::vector<std::function<void()>> idles;
    {
        const std::lock_guard lock(m_mutex);
        idles.swap(m_idles);
    }
    for (const auto& idle : idles)
        idle();
    auto timers = std::move(m_timers);
    m_timers.clear();
    for (const auto& timer : timers) {
        if (timer->cancelled())
            continue;
        if (timer->passed())
            timer->call(timer);
        else
            m_timers.emplace_back(timer);
    }
}

static void run(CLuaState& lua, const std::string& code) {
    if (const auto result = lua.doString(code); !result)
        throw std::runtime_error(result.error());
}

static SP<CLuaState> makeTree(const SP<IEmbeddedBackend>& backend, const SP<IEmbeddedSurface>& surface, lua_Integer color, std::vector<std::string>& errors) {
    auto lua = createLuaState();
    lua->openLibs();
    luaL_requiref(lua->state(), "hyprtoolkit", openModule, 0);
    lua_pop(lua->state(), 1);
    check(attachBackend(lua->state(), backend).has_value(), "attach embedded backend");
    check(setCallbackHooks(lua->state(),
                           {
                               .reportError = [&errors](std::string_view error) { errors.emplace_back(error); },
                           })
              .has_value(),
          "set host error reporter");
    pushElement(lua->state(), surface->rootElement());
    lua_setglobal(lua->state(), "root");
    lua_pushinteger(lua->state(), color);
    lua_setglobal(lua->state(), "tint");
    run(*lua, R"(
        local ht = require("hyprtoolkit")
        clicks = 0
        content = ht.RectangleBuilder.begin()
            :color(function() return ht.color(tint) end)
            :size(ht.dynamicSize(ht.HT_SIZE_PERCENT, ht.HT_SIZE_PERCENT, 1, 1))
            :commence()
        content:setReceivesMouse(true)
        content:setMouseButton(function(_, pressed)
            if pressed then clicks = clicks + 1 end
        end)
        root:addChild(content)
    )");
    lua_getglobal(lua->state(), "content");
    check(getElement(lua->state(), -1).has_value(), "extract Lua-created subtree");
    lua_pop(lua->state(), 1);
    return lua;
}

static void click(const SP<IEmbeddedSurface>& surface) {
    surface->pointerEnter({16, 16});
    surface->pointerMotion({16, 16});
    surface->pointerButton(Input::MOUSE_BUTTON_LEFT, true);
    surface->pointerButton(Input::MOUSE_BUTTON_LEFT, false);
    surface->pointerLeave();
}

static void embeddedTrees() {
    const auto loop    = makeShared<CTestEventLoop>();
    const auto backend = IEmbeddedBackend::create({.common = IBackend::SBackendCreationData{}, .eventLoop = loop});
    check(!!backend, "create embedded backend");
    const Hyprutils::Utils::CScopeGuard shutdown([backend] { backend->destroy(); });
    std::vector<std::string>            errors;
    const auto                          firstSurface  = backend->createSurface();
    const auto                          secondSurface = backend->createSurface();
    check(firstSurface && secondSurface && firstSurface->id() != secondSurface->id(), "create independent embedded surfaces");
    int  frameRequests = 0;
    auto frameListener = firstSurface->m_events.frameRequested.listen([&frameRequests] { ++frameRequests; });
    auto first         = makeTree(backend, firstSurface, 0xFFFF0000, errors);
    auto second        = makeTree(backend, secondSurface, 0xFF00FF00, errors);
    firstSurface->resize({32, 32}, {32, 32}, 1.F);
    secondSurface->resize({32, 32}, {64, 64}, 2.F);
    CFramebuffer firstFramebuffer;
    CFramebuffer secondFramebuffer;
    firstFramebuffer.init(32);
    secondFramebuffer.init(64);
    loop->dispatch();
    firstFramebuffer.render(firstSurface);
    secondFramebuffer.render(secondSurface);
    check(frameRequests > 0, "widget trees request host frames");
    check(firstFramebuffer.pixel() == std::array<GLubyte, 4>{255, 0, 0, 255}, "first Lua tree renders red");
    check(secondFramebuffer.pixel() == std::array<GLubyte, 4>{0, 255, 0, 255}, "second Lua tree renders green at another scale");
    click(firstSurface);
    click(secondSurface);
    click(secondSurface);
    run(*first, "assert(clicks == 1)");
    run(*second, "assert(clicks == 2)");
    check(errors.empty(), "rendering and input callbacks succeed");

    check(retireState(first->state()).has_value(), "retire first script");
    click(firstSurface);
    run(*first, "assert(clicks == 1)");
    first.reset();
    click(firstSurface);
    firstFramebuffer.render(firstSurface);
    firstSurface->rootElement()->clearChildren();
    first = makeTree(backend, firstSurface, 0xFF0000FF, errors);
    firstSurface->resize({32, 32}, {32, 32}, 1.F);
    firstFramebuffer.render(firstSurface);
    check(firstFramebuffer.pixel() == std::array<GLubyte, 4>{0, 0, 255, 255}, "replacement state renders through the existing backend");
    click(firstSurface);
    run(*first, "assert(clicks == 1)");
    run(*second, "assert(clicks == 2)");
    check(errors.empty(), "stale callbacks do not enter the replacement state");

    run(*first, "content:rebuild():color(function() return false end):commence()");
    firstFramebuffer.render(firstSurface);
    check(std::ranges::any_of(errors, [](const auto& error) { return error.contains("color callback"); }), "color conversion errors reach the host");
    errors.clear();
    run(*first, R"(
        local ht = require("hyprtoolkit")
        content:rebuild():color(ht.color(0xFF0000FF)):borderThickness(2)
            :borderGradient(function() return false end):commence()
    )");
    firstFramebuffer.render(firstSurface);
    check(std::ranges::any_of(errors, [](const auto& error) { return error.contains("gradient callback"); }), "gradient conversion errors reach the host");
    check(retireState(first->state()).has_value() && retireState(second->state()).has_value(), "retire scripts before backend shutdown");
}

int main() {
    CEGLContext egl;
    if (!egl.init()) {
        std::cout << "SKIP: no surfaceless EGL GLES3 context available\n";
        return 77;
    }
    try {
        embeddedTrees();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
