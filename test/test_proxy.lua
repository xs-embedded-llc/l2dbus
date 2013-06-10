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
local APP_VER = "1.0.1"

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


local function onCheckPermissions()
	print("\nonCheckPermissions")
end


local function onDeviceAdded(path)
	print("\nonDeviceAdded: " .. path)
end


local function onDeviceRemoved(path)
	print("\nonDeviceRemoved: " .. path)
end


local function onPropertiesChanged(change)
	print("\nonPropertiesChanged:")
	pretty.dump(change)
end


local function onStateChanged(state)
	print("\nonStateChanged: " .. state)
end


local function menuActiveConnections()
	print("==== Active Connections ====")
	local status, result = executeMenuAction(gProxy.p.get.ActiveConnections)
	if not status then
		print("Error: " .. result)
	else
		pretty.dump(unpack(result))
	end
end

local function menuListDevices()
	print("==== Device List ====")
	local status, result = executeMenuAction(gProxy.m.GetDevices)
	if not status then
		print("Error: " .. result)
	else
		pretty.dump(unpack(result))
	end
end

local function menuFindByInterface()
	print("==== Find Device By Interface Name ====")
	io.stdout:write("Enter interface name: ")
	local ifName = gPrompter:getLine()
    local status, result = executeMenuAction(gProxy.m.GetDeviceByIpIface, ifName)
	if not status then
		print("Error: " .. result)
	else
		print("Device: " .. result[1])
	end
end


local function menuDumpIntrospectionData()
	print("==== Introspection Data ====")
	pretty.dump(gProxyCtrl:getIntrospectionData())
end


local function menuListPermissions()
	print("==== Permissions List ====")
	local status, result = executeMenuAction(gProxy.m.GetPermissions)
	if not status then
		print("Error: " .. result)
	else
		pretty.dump(unpack(result))
	end
end

local function menuShowState()
	print("==== NetworkManager State ====")
	local status, result = executeMenuAction(gProxy.m.state)
	if not status then
		print("Error: " .. result)
	else
		print("State: " .. result[1])
	end
end


local function menuShowWirelessState()
	print("==== NetworkManager Wireless State ====")
	local status, result = executeMenuAction(gProxy.p.get.WirelessEnabled)
	if not status then
		print("Error: " .. result)
	else
		print("State: " .. (result[1] and "on" or "off"))
	end
end


local function menuSetWirelessState()
	print("==== NetworkManager Set Wireless State ====")
	io.stdout:write("Enter (t)rue/(f)alse: ")
	local state = gPrompter:getLine()
	state = state:sub(1,1)
	local enable = true
	if (state == "0") or (state == "f") or (state == "n") then
		enable = false
	end
	-- Indicate (true) that no reply is needed for this "set" request
	local status, result = executeMenuAction(gProxy.p.set.WirelessEnabled, enable, true)
	if not status then
		print("Error: " .. result)
	end
end


local function menuGetVersion()
	print("==== NetworkManager Version ====")
	local status, result = executeMenuAction(gProxy.p.get.Version)
	if not status then
		print("Error: " .. result)
	else
		print("Version: " .. result[1])
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


local function menuEnableSignals()
	print("==== NetworkManager Enable/Disable Signals ====")
	io.stdout:write("Enter (e)nable/(d)isable: ")
	local state = gPrompter:getLine()
	state = state:sub(1,1)
	local enable = true
	if (state == "d") or (state == "f") or (state == "n") then
		enable = false
	end

	gProxyCtrl:disconnectAllSignals()
	if enable then
		-- Register signal handlers
		assert(gProxyCtrl:connectSignal("org.freedesktop.NetworkManager",
								"CheckPermissions",
								onCheckPermissions))

		assert(gProxyCtrl:connectSignal("org.freedesktop.NetwormenuToggleBlockingMode()kManager",
								"DeviceAdded",
								onDeviceAdded))

		assert(gProxyCtrl:connectSignal("org.freedesktop.NetworkManager",
								"DeviceRemoved",
								onDeviceRemoved))

		assert(gProxyCtrl:connectSignal("org.freedesktop.NetworkManager",
								"PropertiesChanged",
								onPropertiesChanged))

		assert(gProxyCtrl:connectSignal("org.freedesktop.NetworkManager",
								"StateChanged",
								onStateChanged))
	end
end


local function menuMain()
    local doExit = false
    while not doExit do
        print("\nLua D-Bus Proxy NetworkManager Test Client v"..APP_VER)
        print(string.rep("=", 50))
        print()
        print("a.   Return array of active connections")
        print("b.   Toggle blocking mode (on/off)")
        print("d.   Returns list of all devices")
        print("f.   Find device by interface")
        print("i.   Dump the introspection data")
        print("p.   Returns a list of permissions")
        print("r.   Register/unregister for signals")
        print("s.   Returns the NetworkManager state")
        print("v.   Read the version property")
        print("w.   Read whether wireless is enabled")
        print("W    Enable/disable wireless")
        print("q.   Quit application")
        print(string.rep("=", 50))

        io.stdout:write("\nEnter a command: ")
        local cmd = gPrompter:getChar()
        print("\n")
        if cmd == 'q' then
            doExit = true
        elseif cmd == 'a' then
        	menuActiveConnections()
        elseif cmd == 'b' then
        	menuToggleBlockingMode()
        elseif cmd == 'd' then
        	menuListDevices()
        elseif cmd == 'f' then
        	menuFindByInterface()
        elseif cmd == 'i' then
        	menuDumpIntrospectionData()
        elseif cmd == 'p' then
        	menuListPermissions()
        elseif cmd == 'r' then
        	menuEnableSignals()
        elseif cmd == 's' then
        	menuShowState()
        elseif cmd == 'w' then
        	menuShowWirelessState()
        elseif cmd == 'W' then
        	menuSetWirelessState()
        elseif cmd == 'v' then
        	menuGetVersion()
        end
    end

    gMainLoop:unloop()
end



local function setup()
	l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)
	--l2dbus.Trace.setFlags(l2dbus.Trace.ALL)
	gDispatcher = l2dbus.Dispatcher.new(require("l2dbus_ev").MainLoop.new(gMainLoop))
	assert( nil ~= gDispatcher )
	local conn = l2dbus.Connection.openStandard(gDispatcher, l2dbus.Dbus.BUS_SYSTEM)
	assert( nil ~= conn )

	gProxyCtrl = ProxyController.new(conn, "org.freedesktop.NetworkManager",
											"/org/freedesktop/NetworkManager")
	assert(gProxyCtrl:bind())
	gProxy = gProxyCtrl:getProxy("org.freedesktop.NetworkManager")
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




