#!/usr/bin/env lua
local state = require("utils.luastate")
local ProxyController = require("l2dbus.proxyctrl")
local l2dbus = require("l2dbus")
local pretty = require("pl.pretty")
local ev = require("ev")
local Prompter = require("utils.prompter")

local gMainLoop = ev.Loop.default
-- Must call a method to instantiate the main loop
gMainLoop:now()
local gPrompter = Prompter:new(gMainLoop)
local gDispatcher
local gProxyCtrl
local gProxy
local gPropProxy
local APP_VER = "1.0.0"

--
-- Constants
--
local L2DBUS_TEST_OBJECT = "/org/l2dbus/service/Object"
local L2DBUS_TEST_INTERFACE_NAME = "org.l2dbus.Interface"
local L2DBUS_TEST_BUS_NAME = "org.l2dbus.service.Test"
local L2DBUS_TEST_ERROR_NAME = "org.l2dbus.Error"


local function executeMenuAction(func, ...)
	local bOk, status, result = pcall(func, ...)

	if bOk == false then
        print( "Error: ", status )
        return status, result
    end

	if status then
		if not gProxyCtrl:getBlockingMode() then
			if "userdata" == type(result) then
				local reply, errName, errMsg = gProxyCtrl:waitForReply(result)
				if not reply then
					status = false
					result = tostring(errName) .. " : " .. tostring(errMsg)
				else
					status, result = pcall(reply.getArgsAsArray, reply)
				end
			end
		else
			-- If the result is a message we can extract results then ...
			if "userdata" == type(result) then
				status, result = pcall(result.getArgsAsArray, result)
			end
		end
	end

	return status, result
end


local function onNotifyIntegralTypes(...)
	print("\nonNotifyIntegralTypes")
	print("\tValues: " .. pretty.write({...}))
end


local function onNotifyStringTypes(aString, aObjPath, aSignature)
	print("\nonNotifyStringTypes:")
	print("\tString: " .. aString)
	print("\tObjPath: " .. aObjPath)
	print("\tSignature: " .. aSignature)
end


local function onNotifyComplexStruct(value)
	print("\nonNotifyComplexStruct: " .. pretty.write(value))
end


local function onPropertiesChanged(intfName, changedProps, invalidatedProps)
	print("\nonPropertiesChanged:" .. tostring(intfName))
	print("\tChanged Props: " .. pretty.write(changedProps))
	print("\tInvalidated Props: " .. pretty.write(invalidatedProps))
end


local function menuSendRecvIntegralTypes()
	print("==== SendRecvIntegralTypes ====")
	local values = {1, true, -16, 16, -32, 32, -64, 64, 666.6}
	local status, result = executeMenuAction(gProxy.m.SendRecvIntegralTypes, unpack(values))
	if not status then
		print("Error: " .. result)
	else
		print("Sent: " .. pretty.write(values))
		print("Received: " .. pretty.write(result))
	end
end

local function menuSendRecvStringTypes()
	print("==== SendRecvStringTypes ====")
	local values = {"This is a test string", "/org/object/path", "iiias(iutx)"}
	local status, result = executeMenuAction(gProxy.m.SendRecvStringTypes, unpack(values))
	if not status then
		print("Error: " .. result)
	else
		print("Sent: " .. pretty.write(values))
		print("Received: " .. pretty.write(result))
	end
end


local function menuSendRecvSimpleArray()
	print("==== SendRecvSimpleArray ====")
	local values = {{1,2,4,8,16},
					{"a", "string", "array"},
					{l2dbus.Int64.new(1000),
					l2dbus.Int64.new(-1000),
					l2dbus.Int64.new(123456789)}}
	local status, result = executeMenuAction(gProxy.m.SendRecvSimpleArray, unpack(values))
	if not status then
		print("Error: " .. result)
	else
		print("Sent: " .. pretty.write(values))
		print("Received: " .. pretty.write(result))
	end
end


local function menuSendRecvComplexArray()
	print("==== SendRecvComplexArray ====")
	local values = {{{1,2,3,4,5},{6,7,8,9, 10},{11,12,13,14,15}},
						{{5,"book",{"many", "orange", "tires"}},
						{6,"worm",{"few", "blue", "balloons"}},
						{7,"teacher",{"large", "white", "students"}}},
						{rate=32.4, peak=5, color="purple"}}
	local status, result = executeMenuAction(gProxy.m.SendRecvComplexArray, unpack(values))
	if not status then
		print("Error: " .. result)
	else
		print("Sent: " .. pretty.write(values))
		print("Received: " .. pretty.write(result))
	end
end



local function menuSendRecvComplexStruct()
	print("==== SendRecvComplexStruct ====")
	local values = {{6.6,{"one", "two", "three"},{foo="bar", pi=3.14, list={1,2,3,4}}}}
	local status, result = executeMenuAction(gProxy.m.SendRecvComplexStruct, unpack(values))
	if not status then
		print("Error: " .. result)
	else
		print("Sent: " .. pretty.write(values))
		print("Received: " .. pretty.write(result))
	end
end


local function menuSendDelayedResponse()
	print("==== SendDelayedResponse ====")
	io.stdout:write("Enter amount of delay (msec): ")
	local delay = gPrompter:getLine()
    local status, result = executeMenuAction(gProxy.m.SendDelayedResponse, delay)
	if not status then
		print("Error: " .. result)
	else
		print("Received: " .. tostring(unpack(result)))
	end
end


local function menuQuitRemoteService()
	print("==== QuitRemoteService ====")
	local status, result = executeMenuAction(gProxy.m.quit)
	if not status then
		print("Error: " .. result)
	else
		print("Remote service exited")
	end
end


local function menuDumpIntrospectionData()
	print("==== Introspection Data ====")
	pretty.dump(gProxyCtrl:getIntrospectionData())
end


local function menuSetProperty()
	print("==== Set Property ====")
	io.stdout:write("Enter the property name: ")
	local propName = gPrompter:getLine()
	io.stdout:write("\nEnter the property value: ")
	local value = gPrompter:getLine()
	local status, result = executeMenuAction(gPropProxy.m.Set,
						L2DBUS_TEST_INTERFACE_NAME, propName, tonumber(value))
	if not status then
		print("Error: " .. result)
	else
		print("Property set")
	end
end


local function menuGetProperty()
	print("==== Get Property ====")
	io.stdout:write("Enter the property name: ")
	local propName = gPrompter:getLine()
	local status, result = executeMenuAction(gPropProxy.m.Get,
						L2DBUS_TEST_INTERFACE_NAME, propName)
	if not status then
		print("Error: " .. result)
	else
		print("Value: " .. tostring(result))
	end
end


local function menuGetAllProperties()
	print("==== Get All Properties ====")
	local status, result = executeMenuAction(gPropProxy.m.GetAll, L2DBUS_TEST_INTERFACE_NAME)
	if not status then
		print("Error: " .. result)
	else
		print("Value: " .. pretty.write(result))
	end
end


local function menuToggleBlockingMode()
	print("==== NetworkManager Toggle Proxy Blocking Mode ====")
	io.stdout:write("Enter (e)nable/(d)isable: ")
	local state = gPrompter:getLine()
	state = state:sub(1,1)
	local enable = true
	if (state == "d") or (state == "f") or (state == "n") then
		enable = false
	end
	gProxyCtrl:setBlockingMode(enable)
end


local function menuEnableSignals(enable)
	gProxyCtrl:disconnectAllSignals()
	if enable then
		-- Register signal handlers
		assert(gProxyCtrl:connectSignal(L2DBUS_TEST_INTERFACE_NAME,
								"NotifyIntegralTypes",
								onNotifyIntegralTypes))

		assert(gProxyCtrl:connectSignal(L2DBUS_TEST_INTERFACE_NAME,
								"NotifyStringTypes",
								onNotifyStringTypes))

		assert(gProxyCtrl:connectSignal(L2DBUS_TEST_INTERFACE_NAME,
								"NotifyComplexStruct",
								onNotifyComplexStruct))

		assert(gProxyCtrl:connectSignal(l2dbus.Dbus.INTERFACE_PROPERTIES,
								"PropertiesChanged",
								onPropertiesChanged))
	end
end


local function menuMain()
    local doExit = false
    local enableSignals = true
    while not doExit do
        print("\nLua D-Bus Service Object Test Client v"..APP_VER)
        print(string.rep("=", 50))
        print()
        print("a.   SendRecvIntegralTypes")
        print("b.   SendRecvStringTypes")
        print("c.   SendRecvSimpleArray")
        print("d.   SendRecvComplexArray")
        print("e.   SendRecvComplexStruct")
        print("f.   SendDelayedResponse")
        print("g.   Toggle blocking mode (on/off)")
        print("i.   Dump the introspection data")
        print("j.   Set property")
        print("k.   Get property")
        print("l.   Get all properties")
        print("r.   Register/unregister for signals")
        print("Q.   Tell the remote service to quit/exit")
        print("q.   Quit application")
        print(string.rep("=", 50))

        io.stdout:write("\nEnter a command: ")
        local cmd = gPrompter:getChar()
        print("\n")
        if cmd == 'q' then
            doExit = true
        elseif cmd == 'a' then
        	menuSendRecvIntegralTypes()
        elseif cmd == 'b' then
        	menuSendRecvStringTypes()
        elseif cmd == 'c' then
        	menuSendRecvSimpleArray()
        elseif cmd == 'd' then
        	menuSendRecvComplexArray()
        elseif cmd == 'e' then
        	menuSendRecvComplexStruct()
        elseif cmd == 'f' then
        	menuSendDelayedResponse()
        elseif cmd == 'g' then
        	menuToggleBlockingMode()
        elseif cmd == 'i' then
        	menuDumpIntrospectionData()
        elseif cmd == 'j' then
        	menuSetProperty()
        elseif cmd == 'k' then
        	menuGetProperty()
        elseif cmd == 'l' then
        	menuGetAllProperties()
        elseif cmd == 'r' then
        	menuEnableSignals(enableSignals)
        	enableSignals = not enableSignals
        elseif cmd == 'Q' then
        	menuQuitRemoteService()
        end
    end

    gMainLoop:unloop()
end



local function setup()
	l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)
	--l2dbus.Trace.setFlags(l2dbus.Trace.ALL)
	gDispatcher = l2dbus.Dispatcher.new(require("l2dbus_ev").MainLoop.new(gMainLoop))
	assert( nil ~= gDispatcher )
	local conn = l2dbus.Connection.openStandard(gDispatcher, l2dbus.Dbus.BUS_SESSION)
	assert( nil ~= conn )

	local dbusProxyCtrl = ProxyController.new(conn, l2dbus.Dbus.SERVICE_DBUS, l2dbus.Dbus.PATH_DBUS)
	dbusProxyCtrl:bind(true)
	dbusProxyCtrl:setBlockingMode(true)
	local dbusProxy = dbusProxyCtrl:getProxy(l2dbus.Dbus.INTERFACE_DBUS)

	if not dbusProxy.m.NameHasOwner(L2DBUS_TEST_BUS_NAME) then
    	error("Bus name " .. L2DBUS_TEST_BUS_NAME .. " is not available!")
    end

	gProxyCtrl = ProxyController.new(conn, L2DBUS_TEST_BUS_NAME,
											L2DBUS_TEST_OBJECT)
	assert(gProxyCtrl:bind())

	gProxy = gProxyCtrl:getProxy(L2DBUS_TEST_INTERFACE_NAME)
	gPropProxy = gProxyCtrl:getProxy(l2dbus.Dbus.INTERFACE_PROPERTIES)
end


local function main()
	setup()
    local starter = ev.Idle.new(function(loop, idle, revents)
            idle:stop(loop)
            local co = coroutine.create(menuMain)
            local status, result = coroutine.resume(co)
            if not status then
                print("Error: " .. result)
            end
        end)
    starter:start(gMainLoop)

    gMainLoop:loop()

    gProxyCtrl:disconnectAllSignals()
    gPrompter:restoreTtyState()
    gPrompter = nil
    gProxy = nil
    gProxyCtrl = nil
    gDispatcher = nil
end


main()


gMainLoop = nil
l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
state.dump_stats(io.stdout)




