#!/usr/bin/env lua

local state = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")

local function onTimeout(tm, user)
	print("OnTimeout => Resuming coroutine")
	local res, msg = coroutine.resume(user.co, user.disp, user.conn)
	if res == false then
		print("Error executing command: " .. tostring(msg))
	end
end

local function onNotify(pending, co)
	print("Notified of completed request")
	coroutine.resume(co, pending:stealReply())
end


local function execute(disp, conn)
	local msg = l2dbus.Message.newMethodCall({destination="org.freedesktop.Notifications",
				path="/org/freedesktop/Notifications", interface="org.freedesktop.Notifications",
				method="GetCapabilities"})
	print("Making blocking request . . .")
	local reply, errName, errMsg = conn:sendWithReplyAndBlock(msg)
	if reply ~= nil then
		print("Reply: " .. pretty.write(reply:getArgs()))
	else
		print("Request Failed =>  " .. tostring(errName) .. " : " .. tostring(errMsg))
	end

	-- Let's make a delayed request

	msg = l2dbus.Message.newMethodCall({destination="org.freedesktop.Notifications",
				path="/org/freedesktop/Notifications", interface="org.freedesktop.Notifications",
				method="GetServerInformation"})

	local status, pending = conn:sendWithReply(msg)
	if not status then
		print("Failed to send message")
	else
		pending:setNotify(onNotify, coroutine.running())
		reply = coroutine.yield()
		if pending:isCompleted() and (reply ~= nil ) then
			print("Reply: " .. pretty.write(reply:getArgsAsArray()))
		else
			print("Failed to get a reply to the delayed request")
		end
	end

	print("Exiting out of mainloop")
	disp:stop()
end

local function main()
	l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)
    pretty.dump(l2dbus)

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
    local co = coroutine.create(execute)

    local timeout = l2dbus.Timeout.new(disp, 1000, false, onTimeout, {disp=disp, conn=conn, co=co})
    timeout:setEnable(true)


    print("Starting main loop")
    disp:run(l2dbus.Dispatcher.DISPATCH_WAIT)

    -- Free all resources
    timeout = nil
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
