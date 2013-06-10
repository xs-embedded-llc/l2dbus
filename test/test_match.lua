#!/usr/bin/env lua

local state = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")

local function onTimeout(tm, disp)
	print("Exiting mainloop...")
	disp:stop()
end


local function onFilterMatch(match, msg, ud)
	local msgType = msg:getType()

	io.stdout:write(l2dbus.Message.msgTypeToString(msgType) .. " sender=" ..
					tostring(msg:getSender()) .. " -> dest=" ..
					tostring(msg:getDestination()) .. " serial=" ..
					tostring(msg:getSerial()))
	if (l2dbus.Message.METHOD_CALL == msgType) or
		(l2dbus.Message.SIGNAL == msgType) then
		io.stdout:write(" path=" .. tostring(msg:getObjectPath()) ..
					" interface=" .. tostring(msg:getInterface()) ..
					" member=" .. tostring(msg:getMember()))
	elseif l2dbus.Message.ERROR == msgType then
		io.stdout:write(" name=" .. tostring(msg:getErrorName()))
	end

	local result = msg:getArgsAsArray()
	print()
	for i=1,#result do
		if type(result[i]) == "table" then
			io.stdout:write("table: " .. pretty.write(result[i], "  ") .. "\n")
		else
			print("  " .. type(result[i]) .. ": " .. tostring(result[i]))
		end
	end
end


local function main()
	l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)

	local mainLoop
	if (arg[1] == "--glib") or (arg[1] == "-g") then
		mainLoop = require("l2dbus_glib").MainLoop.new()
	else
		mainLoop = require("l2dbus_ev").MainLoop.new()
	end
	
    local disp = l2dbus.Dispatcher.new(mainLoop)
    assert( nil ~= disp )
    local conn = l2dbus.Connection.openStandard(disp, l2dbus.Dbus.BUS_SESSION)
    assert( nil ~= conn )

    local callFilter = {msgType=l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL,
    					path="/com/canonical/Unity/Panel/Service",
    					interface="com.canonical.Unity.Panel.Service",
    					member="SyncOne",
    					eavesdrop=true,
    					filterArgs={type="string",	-- or "path"
    								index=0,
    								value="libappmenu.so"}
    					}

	local hnd = {}
	hnd[1] = conn:registerMatch(callFilter, onFilterMatch)

    local timeout = l2dbus.Timeout.new(disp, 10000, false, onTimeout, disp)
    timeout:setEnable(true)

    print("Starting main loop -- Open Unity App Menu to Exit")
    disp:run(l2dbus.Dispatcher.DISPATCH_WAIT)

    -- Free all resources
    for i = 1,#hnd do
    	conn:unregisterMatch(hnd[i])
    	hnd[i] = nil
    end
    conn = nil
    disp = nil
end


print("Hit Return to continue")
io.stdin:read("*l")
print("Starting program")
main()

l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
state.dump_stats(io.stdout)
