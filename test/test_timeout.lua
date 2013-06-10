#!/usr/bin/env lua

local state = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")

local tickCount = 0
local gDisp

local function onTimeout(tmout, func)
    tickCount = tickCount + 1
    func("Tick count = " .. tostring(tickCount))
    if tickCount == 5 then
        tmout:setEnable(false)
        gDisp:stop()
    end
end

local function main()
    pretty.dump(l2dbus)

	local mainLoop
	if (arg[1] == "--glib") or (arg[1] == "-g") then
		mainLoop = require("l2dbus_glib").MainLoop.new()
	else
		mainLoop = require("l2dbus_ev").MainLoop.new()
	end
	
    gDisp = l2dbus.Dispatcher.new(mainLoop)

    local timeout = l2dbus.Timeout.new(gDisp, 1000, false, onTimeout, function(str) print(str) end)
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
    gDisp:run(l2dbus.Dispatcher.DISPATCH_WAIT)

    -- Free all resources
    timeout = nil
    gDisp = nil
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
