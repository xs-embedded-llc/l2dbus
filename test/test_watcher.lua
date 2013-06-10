#!/usr/bin/env lua

local state = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")
local bit = require("bit")
local posix = require("posix")

local gDisp

local FIFO_PATH = "/tmp/fifo"
local gReadFd = nil
local gWriteFd = nil
local gTickCnt = 0

local function watchHandler(w, events, user)
    print("WatchHandler called!")
    print("Watch: " .. tostring(w))
    print("Events:")
    pretty.dump(events)
    print("User Data:" .. tostring(user))
    local result = posix.read(gReadFd, 1024)
    print("FIFO data: " .. tostring(result))

    -- Either we've reached an EOF condition or detected an error/hangup
    if 0 == #result then
        w:setEnable(false)
        gDisp:stop()
    end
end


local function onTimeout(tm, disp)
    gTickCnt = gTickCnt + 1
    if gTickCnt % 6 == 0 then
        print("Exiting . . .")
        disp:stop()
    else
        posix.write(gWriteFd, string.format("Tick %d", gTickCnt))
    end
end


local function main()
    local errMsg = nil

    print("Dumping l2dbus_core")
    pretty.dump(l2dbus)

	local mainLoop
	if (arg[1] == "--glib") or (arg[1] == "-g") then
		mainLoop = require("l2dbus_glib").MainLoop.new()
	else
		local ev = require("ev")
		local evLoop = ev.Loop.default
		--local evLoop = ev.Loop.new()
		evLoop:now()
		local backends = { [1] = "select", [2] = "poll", [4] = "epoll",
                       [8] = "kqueue", [16] = "/dev/poll", [32] = "Solaris port"}
    	print("The selected libev backend: " .. backends[evLoop:backend()])
		mainLoop = require("l2dbus_ev").MainLoop.new(evLoop)
	end
	
    gDisp = l2dbus.Dispatcher.new(mainLoop)

    -- Make sure the FIFO is available for reading/writing
    if 0 ~= posix.access(FIFO_PATH, "rw") then
        assert(0 == posix.mkfifo(FIFO_PATH), "Failed to create fifo")
    end

    gReadFd, errMsg = posix.open(FIFO_PATH, bit.bor(posix.O_RDONLY, posix.O_NONBLOCK))
    assert( gReadFd ~= nil, "Failed opening FIFO for reading")

    gWriteFd, errMsg = posix.open(FIFO_PATH, bit.bor(posix.O_WRONLY, posix.O_NONBLOCK))
    assert( gWriteFd ~= nil, "Failed opening FIFO for writing")

    local timer = l2dbus.Timeout.new(gDisp, 2000, true, onTimeout, gDisp)
    timer:setEnable(true)

    local watch = l2dbus.Watch.new(gDisp, gReadFd, l2dbus.Watch.READ, watchHandler, "teststring")

    print("The watch fd is: " .. tostring(watch:getDescriptor()))

    print("The watch events:")
    pretty.dump(watch:events())

    -- The only way to detect a POLLHUP is to watch both read/write events. Since
    -- POLLHUP is mapped to both of these events then they will *both* be set when
    -- such event occurs (even if the client is only writing to the FIFO).
    -- Alternatively no data will be read from the FIFO even though the "read"
    -- event is set.
    print("Setting events: READ/WRITE")
    watch:setEvents("rw")
    watch:setEvents(bit.bor(l2dbus.Watch.READ, l2dbus.Watch.WRITE))
    print("The watch events:")
    pretty.dump(watch:events())

    print("Watch user data: " .. watch:data())
    watch:setData("This is a test")
    print("Watch user data now: " .. watch:data())

    print("The watch is: " .. ((true == watch:isEnabled()) and "enabled" or "disabled"))
    print("Enabling the watch...")
    watch:setEnable(true)
    print("The watch is now: " .. ((true == watch:isEnabled()) and "enabled" or "disabled"))

	local conn = l2dbus.Connection.openStandard(gDisp, l2dbus.Dbus.BUS_SESSION)
    assert( nil ~= conn )
    
    print("Starting main loop")
    gDisp:run(l2dbus.Dispatcher.DISPATCH_WAIT)

    -- Free all resources
    watch = nil
    gDisp = nil
    posix.close(gReadFd)
    posix.close(gWriteFd)
    posix.unlink(FIFO_PATH)
end


print("Hit Return to continue")
io.stdin:read("*l")
print("Starting program")
main()

l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
state.dump_stats(io.stdout)