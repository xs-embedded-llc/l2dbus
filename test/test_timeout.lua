#!/usr/bin/env lua

local state = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")
local ev = require("ev")

local loop = ev.Loop.new()
local tickCount = 0

local function onTimeout(tmout, func)
    tickCount = tickCount + 1
    func("Tick count = " .. tostring(tickCount))
    if tickCount == 5 then
        tmout:setEnable(false)
        loop:unloop()
    end
end

local function main()
    pretty.dump(l2dbus)

    local disp = l2dbus.Dispatcher.new(loop)
    --local disp = l2dbus.Dispatcher.new()

    local timeout = l2dbus.Timeout.new(disp, 1000, false, onTimeout, function(str) print(str) end)
    print("The initialized interval is: " .. timeout:interval())
    timeout:setInterval(2000)
    print("The new interval is: " .. timeout:interval())

    print("The repeat is: " .. ((true == timeout:repeats()) and "on" or "off"))
    timeout:setRepeat(true)
    print("The repeat is now: " .. ((true == timeout:repeats()) and "on" or "off"))

    io.stdout:write("Calling user data: ")
    local func = timeout:data()
    func("Called from user data function\n")
    func = function(s)
        io.stdout:write("=> ")
        io.stdout:write(s .. "\n")
    end
    timeout:setData(func)

    print("The timer is: " .. ((true == timeout:isEnabled()) and "enabled" or "disabled"))
    timeout:setEnable(true)
    print("The timer is: " .. ((true == timeout:isEnabled()) and "enabled" or "disabled"))

    print("Starting main loop")
    loop:loop()

    -- Free all resources
    timeout = nil
    dispatcher = nil
end


print("Hit Return to continue")
io.stdin:read("*l")
print("Starting program")
main()

loop = nil
l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
state.dump_stats(io.stdout)
