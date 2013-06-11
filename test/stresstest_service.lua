#!/usr/bin/env lua

local luaState = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")
local dbusCtrl = require("l2dbus.msgbusctrl")
local svc = require("l2dbus.service")

--
-- Module-scoped globals
--
local gDispatcher = nil
local gService = nil
local gOptions = {payloadSize = 64,
				  replyDelay = 0,
				  replyWithError = false}
local gPayload = string.rep("S", gOptions.payloadSize)
local gTimers = {}


--
-- Constants
--
local L2DBUS_STRESS_TEST_SVC_BUS_NAME = "org.l2dbus.service.StressTest"
local L2DBUS_STRESS_TEST_SVC_OBJECT_PATH = "/org/l2dbus/StressTest"
local L2DBUS_STRESS_TEST_SVC_INTERFACE = "org.l2dbus.StressTest"
local L2DBUS_STRESS_TEST_ERROR_NAME = "org.l2dbus.StressTest.Error"
local L2DBUS_STRESS_TEST_WIRE_METADATA =
[[
<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="org.l2dbus.StressTest">
    <method name="SetOptions">
      <arg direction="in" name="options" type="a{sv}"/>
    </method>
    <method name="Echo">
      <arg direction="in" name="payload" type="s"/>
      <arg direction="out" name="payload" type="s"/>
    </method>
    <method name="Test">
      <arg direction="in" name="seq" type="u"/>
      <arg direction="in" name="payload" type="s"/>
      <arg direction="out" name="seq" type="u"/>
      <arg direction="out" name="payload" type="s"/>
    </method>
    <method name="Quit"/>    
  </interface>
</node>
]] 


--
-- Signal Handlers for D-Bus Proxy
--

local function onNameOwnerChanged(busName, oldOwner, newOwner)
	print("\nonNameOwnerChanged:")
	print("\tBusName: " .. busName)
	print("\tOldOwner: " .. oldOwner)
	print("\tNewOwner: " .. newOwner)
end


local function onNameLost(name)
	print("\nonNameLost: " .. name)
end


local function onNameAcquired(name)
	print("\nonNameAcquired: " .. name)
end


local function reply(ctx, ...)
	local params = {...}
	local function onReplyLater(tm)
		if gOptions.replyWithError then
			ctx:error(L2DBUS_STRESS_TEST_ERROR_NAME, gPayload)
		else
			ctx:reply(unpack(params))
		end
	end

	if gOptions.replyDelay > 0 then
		local timeout = l2dbus.Timeout.new(gDispatcher, gOptions.replyDelay,
											false, onReplyLater, nil)		
	    timeout:setEnable(true)
	else
		if gOptions.replyWithError then
			ctx:error(L2DBUS_STRESS_TEST_ERROR_NAME, gPayload)
		else
			ctx:reply(...)
		end
	end
end


--
-- Service handlers
--

local function defaultSvcHandler(ctx, intfName, member, args)
	if member == "Quit" then
		ctx:reply()
		ctx:getConnection():flush()
		gDispatcher:stop()
		io.stderr:write("Server received message to quit . . . exiting!\n")
	else
		ctx:error(L2DBUS_STRESS_TEST_ERROR_NAME, "unhandled request for " ..
				((intfName == nil) and "" or (intfName .. ".")) .. member)
	end
end


local function onSetOptions(ctx, options)
	gOptions = options
	if gOptions.payloadSize >= 0 then
		gPayload = string.rep("G", gOptions.payloadSize)
		ctx:reply()
	else
		ctx:error(L2DBUS_STRESS_TEST_ERROR_NAME, "Payload size must be >= 0")
	end
end


local function onEcho(ctx, payload)
	reply(ctx, payload)
end

local function onTest(ctx, seq, payload)
	reply(ctx, seq, gPayload)
end



local function main()
	l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)
	local result
	local status

	local mainLoop
	if (arg[1] == "--glib") or (arg[1] == "-g") then
		mainLoop = require("l2dbus_glib").MainLoop.new()
	else
		mainLoop = require("l2dbus_ev").MainLoop.new()
	end
	
    gDispatcher = l2dbus.Dispatcher.new(mainLoop)
    assert( nil ~= gDispatcher )    
    local conn = l2dbus.Connection.openStandard(gDispatcher, l2dbus.Dbus.BUS_SESSION)
    assert( nil ~= conn )
    
	print(string.format("Connection: max message size: %d (bytes)",
			conn:getMaxMessageSize()))
	print(string.format("Connection: max received size: %d (bytes)",
			conn:getMaxReceivedSize()))
	
    local dbusProxyCtrl = dbusCtrl.new(conn)
    dbusProxyCtrl:bind(true)
    dbusProxyCtrl:setBlockingMode(true)
    
    -- Register for the D-Bus signals
    assert(dbusProxyCtrl:connectSignal("NameOwnerChanged",
								onNameOwnerChanged))
    assert(dbusProxyCtrl:connectSignal("NameLost",
								onNameLost))
    assert(dbusProxyCtrl:connectSignal("NameAcquired",
								onNameAcquired))

	local  dbusProxy = dbusProxyCtrl:getProxy()
    
    status, result = dbusProxy.m.NameHasOwner(L2DBUS_STRESS_TEST_SVC_BUS_NAME)
    assert(status)																
    if result then
    	error("Bus name " .. L2DBUS_STRESS_TEST_SVC_BUS_NAME .. " is already claimed!")
    end
    
    status, result = dbusProxy.m.RequestName(L2DBUS_STRESS_TEST_SVC_BUS_NAME,
    										l2dbus.Dbus.NAME_FLAG_DO_NOT_QUEUE)
    assert( status )
    assert( result == l2dbus.Dbus.REQUEST_NAME_REPLY_PRIMARY_OWNER )
     
    
    -- Register the interface
    gService = svc.new(L2DBUS_STRESS_TEST_SVC_OBJECT_PATH, true, defaultSvcHandler)
    assert( gService:addInterface(L2DBUS_STRESS_TEST_SVC_INTERFACE,
    								L2DBUS_STRESS_TEST_WIRE_METADATA) )
    
    gService:registerMethodHandler(L2DBUS_STRESS_TEST_SVC_INTERFACE,
    										"Echo",
    										onEcho)

    gService:registerMethodHandler(L2DBUS_STRESS_TEST_SVC_INTERFACE,
    										"Test",
    										onTest)

    gService:registerMethodHandler(L2DBUS_STRESS_TEST_SVC_INTERFACE,
    										"SetOptions",
    										onSetOptions)
										    										    										    										    										    										    										    
    -- Bind the service object to the connection
    gService:attach(conn)

    print("Starting main loop")
    gDispatcher:run(l2dbus.Dispatcher.DISPATCH_WAIT)
    
    gService:detach(conn)
    dbusProxyCtrl:setBlockingMode(true)
    dbusProxyCtrl:disconnectAllSignals()
    status, result = dbusProxy.m.ReleaseName(L2DBUS_STRESS_TEST_SVC_BUS_NAME)
    assert(l2dbus.Dbus.RELEASE_NAME_REPLY_RELEASED == result)
    conn:flush()
    gService = nil
    conn = nil
    gDispatcher = nil    
end


--print("Hit Return to continue")
--io.stdin:read("*l")
print("Starting program")
main()

l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
luaState.dump_stats(io.stdout)
