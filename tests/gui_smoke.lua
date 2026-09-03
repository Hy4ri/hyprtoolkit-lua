local ht = assert(hyprtoolkit)
local backend, backendError = ht.Backend.create()
assert(backend, backendError)
backend:setLogFn(function(_, _) end)
backend:setLogFn(nil)

local window = assert(ht.WindowBuilder.begin()
    :preferredSize(ht.vec2(320, 120))
    :appTitle("Hyprtoolkit Lua smoke test")
    :appClass("hyprtoolkit-lua-smoke")
    :commence())

local text = ht.TextBuilder.begin()
    :text("Hello from Lua")
    :align(ht.HT_FONT_ALIGN_CENTER)
    :size(ht.dynamicSize(ht.HT_SIZE_PERCENT, ht.HT_SIZE_PERCENT, 1, 1))
    :commence()

window:root():addChild(text)
local closeConnection = window:onCloseRequest(function()
    window:close()
    backend:destroy()
end)

local timer
local cancelledTimer
local callbackThread = coroutine.create(function()
    cancelledTimer = backend:addTimer(10000, function()
        return cancelledTimer
    end)
    cancelledTimer:cancel()
    timer = backend:addTimer(250, function()
        window:close()
        backend:destroy()
    end)
end)
assert(coroutine.resume(callbackThread))
assert(timer and closeConnection:connected())

window:open()
backend:enterLoop()

assert(not pcall(function()
    backend:getOutputs()
end))
assert(not pcall(function()
    window:open()
end))
