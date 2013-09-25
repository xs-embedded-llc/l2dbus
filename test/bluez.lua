#!/usr/bin/env lua
----------------------------------------------------------------
--- BlueZ Tester
---
--- This is a simple client for exercising parts of BlueZ.  This
--- code is meant to exercise various parts of the l2dbus API
--- and prove the code works with other 3rd party services. This
--- tool is NOT meant to be an exhaustive BlueZ tool.
---
--- The current implementation supports:
--- + start/stop discovery
--- + view adapters
--- + view adapter info/properties
--- + view/create/remove/pair devices (we can act as the agent for pairing)
--- + AudioSource support (plays A2DP output from device)
--- + PAN support (NOTE: Need to manually start DHCP--see "Simple Testing Procedures" below)
--- + HFP support (Needs more work)
---
--- NOTE: Tested with Ubuntu Bluez v4.98 & 4.101
---
--- Some Limitations:
--- =============================
--- 1. agent only set on CreatePairedDevice.  So if pairing is initiated
---    from the phone, there will be no agent to handle this.  Pairing
---    MUST be initiated from this app.
--- 2. Patches required for 4.101 PANU to connect to NAP.
--- 3. See occassionaly hang after pairing and the agent is released.
---    Seems better if you Confirm the request from this program first,
---    then accept the pairing on the phone.
---    Workaround: kill this app and restart.
---
--- System Setup:
--- =============================
--- This MUST run on the dbus system bus, therefore you must open
--- access for this service.  For example, add the following file(s):
--- /etc/dbus-1/system.d/com.service.TestBlueZ.conf
---
--- <!-- This configuration file specifies the required security policies
---      for oFono core daemon to work. -->
---
--- <!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
---  "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
--- <busconfig>
---
---   <!-- ../system.conf have denied everything, so we just punch some holes -->
---
---   <policy user="root">
---     <allow own="com.service.TestBlueZ"/>
---     <allow send_destination="com.service.TestBlueZ"/>
---     <allow send_interface="org.bluez.Agent"/>
---     <allow send_interface="org.bluez.HandsfreeAgent"/>
---   </policy>
---
---   <policy at_console="true">
---     <allow send_destination="com.service.TestBlueZ"/>
---   </policy>
---
---   <policy context="default">
---     <allow send_destination="com.service.TestBlueZ"/>
---   </policy>
---
--- </busconfig>
---
---
--- Simple Testing Procedures:
--- =============================
---
--- Start Discovery of Devices:
--- ---------------------------
--- 1. Option a ("Adapter menu...")
---    8 ("Start/stop Discovery")
---    <Enter>, ("Start" is default)
---    <Enter>
--- 2. Watch as devices are found and info listed
---
--- Stop Discovery of Devices:
--- ---------------------------
--- 1. Option a ("Adapter menu...")
---    8 ("Start/stop Discovery")
---    <Enter>
---    ("Stop" is default) <Enter>
--- 2. Now the discovery will have stopped
---
--- Pair a Devices:
--- ---------------------------
--- 1. Do, "Start Discovery of Devices" from above
--- 2. Do, "Stop Discovery of Devices" from above
--- 3. Option 3 ("CreatePairedDevice...") <Enter>
--- 4. Select the device in the list of found devices,
---    <Enter>
---    ("confirmation") <Enter>
---
--- Connect to PAN NAP on Phone: (Tested with iPhone 5)
--- ----------------------------
--- 1. Do, "Pair a Devices" from above
--- 2. From main menu, Option d ("Device menu...")
---    6 ("Connect to Service...")
---    <Enter>
---    2 ("org.bluez.Network")
---    ("Connect" is default) <Enter>
---    ("nap" is default) <Enter>
---
----------------------------------------------------------------
local proxyctrl = require("l2dbus.proxyctrl")
local l2dbus    = require("l2dbus")
local pretty    = require("pl.pretty")
local stringx   = require("pl.stringx")
local Prompter  = require("utils.prompter")

-- Const
--------
local APP_VER       = "2.1.0"

local BUSNAME       = 1
local OBJPATH       = 2
local IFACE         = 3
local BLUEZ_MGR     = {"org.bluez", "/", "org.bluez.Manager"}
local BLUEZ_ADAPTER = {"org.bluez", nil, "org.bluez.Adapter"} -- objPath filled in later
local BLUEZ_CTRL    = {"org.bluez", nil, "org.bluez.Control"} -- objPath filled in later
local BLUEZ_DEVICE  = {"org.bluez", nil, "org.bluez.Device"}  -- objPath filled in later

-- Service API for BlueZ Agents
local SVC              = {"com.service.TestBlueZ",      "/com/service/TestBlueZ",      nil}
local AGENT            = {"com.service.TestBlueZAgent", "/com/service/TestBlueZAgent", nil}
local PAIRING_AGENT    = {AGENT[BUSNAME], AGENT[OBJPATH], "org.bluez.Agent"}
local HFP_AGENT        = {AGENT[BUSNAME], AGENT[OBJPATH], "org.bluez.HandsfreeAgent"}
local OFONO_HFP_AGENT  = {"org.ofono",    "/",            "org.ofono.Manager"}
local OFONO_HFP_MODEM  = {"org.ofono",    nil,            "org.ofono.Modem"}


local DEFAULT_PASSKEY       = 0
local DEFAULT_PINCODE       = "000000"

-- Methods we register for the pairing process
local PAIRING_AGENT_METHODS = {

    { name = "Authorize",
      args = {
             {name = "device",  sig="o", dir="in"},
             {name = "uuid",    sig="s", dir="in"} }
    },
    { name = "Cancel",
      args = {  }
    },
    { name = "ConfirmModeChange",
      args = {
             {name = "mode",  sig="s", dir="in"} }
    },
    { name = "DisplayPasskey",
      args = {
             {name = "device",  sig="o", dir="in"},
             {name = "passkey", sig="u", dir="in"},
             {name = "entered", sig="y", dir="in"} }
    },
    { name = "DisplayPinCode",
      args = {
             {name = "device",  sig="o", dir="in"},
             {name = "pincode", sig="s", dir="in"} }
    },
    { name = "Release",
      args = { }
    },
    { name = "RequestConfirmation",
      args = {
             {name = "device",  sig="o", dir="in"},
             {name = "passkey", sig="u", dir="in"} }
    },
    { name = "RequestPasskey",
      args = {
             {name = "device",  sig="o", dir="in"},
             {name = "pinCode", sig="u", dir="out"} }
    },
    { name = "RequestPinCode",
      args = {
             {name = "device",  sig="o", dir="in"},
             {name = "pinCode", sig="s", dir="out"} }
    },
}

-- Methods we register for HFP
local HFP_AGENT_METHODS = {

    { name = "NewConnection",
      args = {
             {name = "fd",       sig="h", dir="in"},
             {name = "version",  sig="s", dir="in"} }
    },
    { name = "Release",
      args = {  }
    }
}

-- Globals
----------

local gDispatcher = l2dbus.Dispatcher.new(require("l2dbus_ev").MainLoop.new())
local gPrompter   = Prompter.getInstance(gDispatcher)
local gProxyCtrl
local gMgrProxy
local gSystemConn

local gOfonoProxy
local gOfonoProxyCtrl

-- Agent Service Specific
local gProxySvc
local gAgentSvc
local gPairInf
local gHfpInf
local gHfpMethods    = {} -- external dbus API for HFP
local gPairMethods   = {} -- external dbus API for pairing

local gDiscoveryList = {} -- macAddr/object for each discovered device

-- option 'z' related variables
local gLastFunct     = nil
local gLastArgs      = nil
local gLastInfo      = ""

local gLastAdapter   = nil -- last selected adapter
local gLastDevice    = nil -- last selected device
local gLastService   = nil -- last selected service

-- skips IO and tracking the last command for a single call to executeMenuAction
local gSilentExecute  = false

-- prompts in executeMenuAction() to confirm the execution
local gConfirmExecute = false


----------------------------------------------------------------
--- dumpMessage
---
--- Dump a DBus message (output format similar to dbus-monitor)
---
--- @tparam   (userdata)  msg ....
----------------------------------------------------------------
local function dumpMessage(msg)
    -- Dump out the message
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

    io.stdout:flush( )

end -- dumpMessage

----------------------------------------------------------------
--- executeMenuAction
---
--- [add description here]
---
--- @tparam   (string)  functName ....
--- @tparam   (string)  func      ....
--- @tparam   (string)  ...       ....
---
--- @treturn  (boolean) [add here]
----------------------------------------------------------------
local function executeMenuAction(functName, func, ...)

    if gSilentExecute == false then

        -- Tracking for executing the last command easier
        gLastFunct = executeMenuAction
        gLastArgs  = { functName, func, ... }
        gLastInfo  = string.format( "%s(%s)", functName, (#gLastArgs>2) and pretty.write(...) or "")
    end

    if gConfirmExecute == true then
        gConfirmExecute = false
        print(string.format( "\nCalling: %s(%s)", functName, (#gLastArgs>2) and pretty.write(...) or ""))
        print( "Continue Request? (Y)es or (N)o [default is No]" )
        local cmd = gPrompter:getChar()
        print("\n")
        if cmd ~= "y" and cmd ~= "Y" then
            print( "Aborting..." )
            gSilentExecute = false
            return
        end
    end

    if gSilentExecute == false then
        print("\nCalling:", functName)
    end

    local bOk, status, result = pcall(func, ...)  -- EXECUTE METHOD

    --[[
    if gSilentExecute == false then
        if type(result) == "table" then
            print(functName, status, result, pretty.write(result))
        else
            print(functName, status, result)
        end
    end ]]

    if bOk == false then
        if gSilentExecute == false then
            print( "Error: ", status )
        end
        gSilentExecute = false
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

    if gSilentExecute == false then
        print( "Result:" )
    end

    if not status then
        if gSilentExecute == false then
            print("Error: " .. result)
        end
        gSilentExecute = false
        return status, result

    elseif type(result) == "table" then
        -- Always a array because of getArgsAsArray
        local unpackedResult = unpack( result )
        if gSilentExecute == false then
            if type(unpackedResult) == "table" then
                pretty.dump( unpackedResult )
            else
                print( unpackedResult )
            end
        end
        gSilentExecute = false
        return status, unpackedResult
    end

    -- Not likely unless a problem exists
    if gSilentExecute == false then
        print( result )
    end
    gSilentExecute = false
    return status, result

end -- executeMenuAction


----------------------------------------------------------------
--- onAgentRequest
---
--- Simple generic handler for requests.
---
--- @tparam   (string)  prefix   ...."pairing"|"hfp"
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
---
--- @treturn  Always l2dbus.Dbus.HANDLER_RESULT_HANDLED
----------------------------------------------------------------
local function onAgentRequest(prefix, ifaceObj, conn, msg, userdata)

    local member  = msg:getMember()
    local methods = gPairMethods

    print("\nonAgentRequest: " .. ifaceObj:name() .. "/" .. member, prefix )
    dumpMessage(msg)

    if prefix == "hfp" then
        methods = gHfpMethods
    end

    if methods[prefix.."_"..member] == nil then
        local sErr = string.format("ERROR: Unknown %s method: %s", prefix, member)
        print( sErr  )
        conn:send( l2dbus.Message.newError( msg, "ERR_INV", sErr ) )
        return l2dbus.Dbus.HANDLER_RESULT_NOT_YET_HANDLED;
    end

    methods[prefix.."_"..member]( ifaceObj, conn, msg, userdata )

    return l2dbus.Dbus.HANDLER_RESULT_HANDLED;

end -- onAgentRequest

----------------------------------------------------------------
--- defaultSvcHandler
---
--- Default service handler for requests.  Only logs.
---
--- @tparam   (string)  svcObj   ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
---
--- @treturn  Always l2dbus.Dbus.HANDLER_RESULT_HANDLED
----------------------------------------------------------------
local function defaultSvcHandler(svcObj, conn, msg, userdata)

    print("\nDefault API: " .. svcObj:path() .. "/" .. msg:getMember() )
    dumpMessage(msg)

    conn:send( l2dbus.Message.newMethodReturn( msg ) )

    return l2dbus.Dbus.HANDLER_RESULT_HANDLED;

end -- defaultSvcHandler

----------------------------------------------------------------
--- initDbus
---
--- [add description here]
----------------------------------------------------------------
local function initDbus()

    l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)
    --l2dbus.Trace.setFlags(l2dbus.Trace.ALL)

    assert( nil ~= gDispatcher )

    -- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    local function requestName( sysConn, svcName )
        local msg = l2dbus.Message.newMethodCall({destination = l2dbus.Dbus.SERVICE_DBUS,
                                                  path        = l2dbus.Dbus.PATH_DBUS,
                                                  interface   = l2dbus.Dbus.INTERFACE_DBUS,
                                                  method      = "RequestName"})
        msg:addArgsBySignature("su", svcName, l2dbus.Dbus.NAME_FLAG_DO_NOT_QUEUE)

        print("Requesting a system bus name: " .. svcName)
        local reply, errName, errMsg = sysConn:sendWithReplyAndBlock( msg )
        if reply == nil then
            error("Request Failed =>  " .. tostring(errName) .. " : " .. tostring(errMsg))
        else
            local result = reply:getArgs()
            assert( result == l2dbus.Dbus.REQUEST_NAME_REPLY_PRIMARY_OWNER )
            print("Acquired name successfully on system bus")
        end
    end -- requestName
    -- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    local function initMainProxy()

        -- Init Proxy to BlueZ
        ------------------------------
        gSystemConn = l2dbus.Connection.openStandard(gDispatcher, l2dbus.Dbus.BUS_SYSTEM)
        assert( nil ~= gSystemConn )

        requestName( gSystemConn, SVC[BUSNAME] )

        gProxySvc = l2dbus.ServiceObject.new( SVC[OBJPATH],
                                              defaultSvcHandler,
                                              "DefaultHandler" )
        -- Introspection interface
        assert( gProxySvc:addInterface( l2dbus.Introspection.new() ) )

        assert( gSystemConn:registerServiceObject( gProxySvc ) )

        -- Setup proxy to bluez
        gProxyCtrl = proxyctrl.new(gSystemConn, BLUEZ_MGR[BUSNAME], BLUEZ_MGR[OBJPATH] )
        assert(gProxyCtrl:bind())

        gMgrProxy = gProxyCtrl:getProxy( BLUEZ_MGR[IFACE] )
        assert( nil ~= gMgrProxy )

    end -- initMainProxy
    -- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    local function initAgent()

        -- Init BlueZ Agents Interface
        ------------------------------
        gSystemAgentConn = gSystemConn --l2dbus.Connection.openStandard(gDispatcher, l2dbus.Dbus.BUS_SYSTEM)
        assert( nil ~= gSystemAgentConn )

        --arequestName( gSystemAgentConn, AGENT[BUSNAME] )

        gAgentSvc = l2dbus.ServiceObject.new( AGENT[OBJPATH],
                                              defaultSvcHandler,
                                              "DefaultHandler" )
        -- Pairing Interface
        gPairInf = l2dbus.Interface.new( PAIRING_AGENT[IFACE],
                                         function (ifaceObj, conn, msg, userdata)
                                             return onAgentRequest( "pairing", ifaceObj, conn, msg, userdata )
                                         end,
                                         "PairingAgentHandler" )
        gPairInf:registerMethods( PAIRING_AGENT_METHODS )
        assert( gAgentSvc:addInterface( gPairInf ) )

        -- HFP Interface
        gHfpInf = l2dbus.Interface.new( HFP_AGENT[IFACE],
                                        function (ifaceObj, conn, msg, userdata)
                                            return onAgentRequest( "hfp", ifaceObj, conn, msg, userdata )
                                        end,
                                        "HfpAgentHandler" )
        gHfpInf:registerMethods( HFP_AGENT_METHODS )
        assert( gAgentSvc:addInterface( gHfpInf ) )

        -- Introspection interface
        assert( gAgentSvc:addInterface( l2dbus.Introspection.new() ) )

        assert( gSystemAgentConn:registerServiceObject( gAgentSvc ) )

    end -- initAgent
    -- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    initMainProxy( )

    initAgent(  )

end -- initDbus

----------------------------------------------------------------
--- initOfono
---
--- [add description here]
---
--- @tparam   (string)  proxy ....
---
--- @treturn  (boolean) [add here]
----------------------------------------------------------------
local function initOfono( proxy )

    if gOfonoProxy == nil then

        gOfonoProxyCtrl = proxyctrl.new(gSystemConn, OFONO_HFP_AGENT[BUSNAME], OFONO_HFP_AGENT[OBJPATH] )
        if gOfonoProxyCtrl == nil then
            print( "INFO: No ofono support. Using our internal service." )
            return false
        end
        assert(gOfonoProxyCtrl:bind())

        gOfonoProxy = gOfonoProxyCtrl:getProxy( OFONO_HFP_AGENT[IFACE] )
        assert( nil ~= gOfonoProxy )
    end

    gSilentExecute = true
    local status, tModems = executeMenuAction( "GetModems", gOfonoProxy.m.GetModems )

    if status == false then
        print( "ERROR: Unable to get the modems list." )
        return false
    end

    if #tModems == 0 then
        print( "INFO: No modems detected." )
        return false
    end

    -- Find the matching modem without
    -- user interaction since they already
    -- selected it
    ------------------------------
    local sMacAddr   = string.match( gLastDevice or "", "/dev_(%S*)" )
    local sSelDevice = string.gsub( sMacAddr, "_", "" )
    local sUseModem  = nil
    for _,v in pairs(tModems) do
        if string.find(v[1], sSelDevice) then
            sUseModem = v[1]
            break
        end
    end

    if sUseModem == nil then
        return
    end

    print( "Using ofono modem path: ", sUseModem )

    -- Power on Modem
    ------------------------------
    local proxyCtrl = proxyctrl.new(gSystemConn, OFONO_HFP_MODEM[BUSNAME], sUseModem )
    if proxyCtrl == nil then
        print( "INFO: Could not access modem for: ", sUseModem )
        return false
    end
    assert(proxyCtrl:bind())

    local proxy = proxyCtrl:getProxy( OFONO_HFP_MODEM[IFACE] )
    assert( nil ~= proxy )

    executeMenuAction( "SetProperty", proxy.m.SetProperty,
                       "Powered", l2dbus.DbusTypes.Variant.new(true, "vb") )
    proxyCtrl = nil
    proxy     = nil

    -- Register
    ------------------------------
    local status, result = executeMenuAction( "RegisterAgent", proxy.m.RegisterAgent, OFONO_HFP_AGENT[OBJPATH], "DisplayYesNo" )
    if status == false then
        if string.find(result, "AlreadyExists") == nil then
            print( "ERROR: Unable to register the ofono HFP agent." )
            return false
        end
    end

    return true

end -- initOfono

----------------------------------------------------------------
--- getAdapterProxy
---
--- [add description here]
---
---
--- @treturn  (boolean) [add here]
----------------------------------------------------------------
local function getAdapterProxy()

    gSilentExecute = true
    local status, tAdapters = executeMenuAction( "ListAdapters", gMgrProxy.m.ListAdapters )

    if status == false then
        print( "ERROR: Unable to get the adapter list." )
        return nil
    end

    if #tAdapters == 0 then
        print( "INFO: No adapters detected." )
        return nil
    end

    -- If only one entry, don't bother asking
    if #tAdapters ~= 1 then
        table.insert( tAdapters, "Cancel Operations" )
        local sAdapter = gPrompter:selection( gLastAdapter or {}, "Select an adapter", tAdapters )
        if sAdapter and sAdapter ~= "Cancel Operations" then
            gLastAdapter = sAdapter
        else
            print( "Aborting..." )
            return nil
        end
    else
        gLastAdapter = tAdapters[1]
        print( "\nSelected Adapter: ", gLastAdapter, "\n" )
    end

    local proxyCtrl = proxyctrl.new(gSystemConn, BLUEZ_ADAPTER[BUSNAME], gLastAdapter )
    assert(proxyCtrl:bind())

    local proxy = proxyCtrl:getProxy( BLUEZ_ADAPTER[IFACE] )
    assert( nil ~= proxy )

    return proxy, proxyCtrl

end -- getAdapterProxy

----------------------------------------------------------------
--- getDeviceIfaceProxy
---
--- [add description here]
---
--- @tparam   (string)  iface          ....BLUEZ_CTRL | BLUEZ_DEVICE
--- @tparam   (string)  adapterProxy   ....non-nil will skip the adapter selection
--- @tparam   (string)  bUseLastDevice ....non-nil will skip the device selection
---
--- @treturn  (userdata) proxy to interface
--- @treturn  (userdata) adapter proxy
--- @treturn  (userdata) device proxy
----------------------------------------------------------------
local function getDeviceIfaceProxy( iface, adapterProxy, bUseLastDevice )

    if adapterProxy == nil then
        adapterProxy = getAdapterProxy()
        if adapterProxy == nil then
            return
        end
    end

    if bUseLastDevice ~= true then
        gSilentExecute = true
        local status, tDeviceList = executeMenuAction( "ListDevices", adapterProxy.m.ListDevices )

        if status == false then
            print( "ERROR: Unable to get the created device list." )
            return nil
        end

        if #tDeviceList == 0 then
            print( "INFO: No created devices detected" )
            return nil
        end

        -- If only one entry, don't bother asking
        if #tDeviceList ~= 1 then
            table.insert( tDeviceList, "Cancel Operations" )
            local sDevice = gPrompter:selection( gLastDevice or {}, "Select a device", tDeviceList )
            if sDevice and sDevice ~= "Cancel Operations" then
                gLastDevice = sDevice
            else
                print( "Aborting..." )
                return nil
            end
        else
            gLastDevice = tDeviceList[1]
            print( "\nSelected Device: ", gLastDevice, "\n" )
        end
    end

    local proxyCtrl = proxyctrl.new(gSystemConn, iface[BUSNAME], gLastDevice )
    assert(proxyCtrl:bind())

    --local proxy = proxyCtrl:getProxy( iface[IFACE] )
    local proxy = proxyCtrl:getProxy("org.bluez.Device")
    assert( nil ~= proxy )

    return proxy, adapterProxy, proxyCtrl

end -- getDeviceIfaceProxy


----------------------------------------------------------------
--- menuConnectService
---
--- [add description here]
---
----------------------------------------------------------------
local function menuConnectService( proxyCtrl )

    local tUserChoices = {}
    local tIntrospect  = proxyCtrl:getIntrospectionData()

    -- Build Menu List
    for k,v in pairs(tIntrospect)  do
        -- Ignore some non-service interfaces
        if string.find(k, "Introspectable") == nil      and
           string.find(k, "org%.bluez%.Control") == nil and
           string.find(k, "org%.bluez%.Device") == nil  and
           ( v.methods and (v.methods.Connect and v.methods.Disconnect)) then
            table.insert( tUserChoices, k )
        end
    end
    table.insert( tUserChoices, "Cancel Operation" )

    local sIface = gPrompter:selection( gLastService or {},
                                        "Select a service:",
                                        tUserChoices )
    if sIface == nil or sIface == "Cancel Operation" then
        print( "Aborting..." )
        return
    end
    gLastService = sIface

    tUserChoices = { "Connect", "Disconnect", "Cancel Operation" }

    --pretty.dump( tIntrospect )

    if tIntrospect[sIface].methods == "GetProperties" then
        table.insert( tUserChoices, #tUserChoices, "GetProperties" )
    end

    local proxy = proxyCtrl:getProxy( sIface )

    if tIntrospect[sIface].signals.PropertyChanged then
        -- Track the changes that are signalled
        proxyCtrl:connectSignal( sIface,
                                 "PropertyChanged",
                                 function ( sProperty, val )
                                     if type(val) == "table" then
                                         print( "\nPropertyChanged: ", sProperty, pretty.write(val) )
                                     else
                                         print( "\nPropertyChanged: ", sProperty, val )
                                     end
                                 end )
    end

    if sIface == "org.bluez.Network" then

        local sOpt = gPrompter:selection( nil, "Select a method:", tUserChoices )
        if sOpt == nil or sOpt == "Cancel Operation" then
            print( "Aborting..." )
            return
        end

        if sOpt == "Connect" then
            local sUUID = gPrompter:selection( nil, "Select UUID:",
                                               {"nap", "gn", "panu", "custom", "Cancel Operation"} )
            if sUUID == nil or sUUID == "Cancel Operation" then
                print( "Aborting..." )
                return
            end
            if sUUID == "custom" then
                io.stdout:write("Enter a UUID: ")
                sUUID = gPrompter:getLine()
                print( "\n" )
            end

            executeMenuAction( sOpt, proxy.m[sOpt], sUUID )

        else
            executeMenuAction( sOpt, proxy.m[sOpt] )
        end

    elseif sIface == "org.bluez.HandsfreeGateway" then

        local sOpt = gPrompter:selection( nil, "Select a method:", tUserChoices )
        if sOpt == nil or sOpt == "Cancel Operation" then
            print( "Aborting..." )
            return
        end

        if sOpt == "Connect" then

            --TODO: MUST track and use gLastHfpRegAgent
            --executeMenuAction( "UnregisterAgent", proxy.m.UnregisterAgent, gLastHfpRegAgent )

            -- MUST first register an agent
            -- First look for ofono, if this is available, let it be the agent
            if initOfono( proxy ) == false then

                local status, result = executeMenuAction( "RegisterAgent", proxy.m.RegisterAgent, HFP_AGENT[OBJPATH], "DisplayYesNo" )
                if status == false then
                    if string.find(result, "AlreadyExists") == nil then
                        print( "ERROR: Unable to register an HFP agent." )
                        return
                    end
                end
            end

        end

        executeMenuAction( sOpt, proxy.m[sOpt] )

    elseif sIface == "org.bluez.Serial" then

        -- Only Connect/Disconnect (no parameters)

        local sOpt = gPrompter:selection( nil, "Select a method:", tUserChoices )
        if sOpt == nil or sOpt == "Cancel Operation" then
            print( "Aborting..." )
            return
        end

        executeMenuAction( sOpt, proxy.m[sOpt] )

    else -- "org.bluez.Audio", "org.bluez.AudioSource", etc

        -- Only Connect/Disconnect (no parameters)

        local sOpt = gPrompter:selection( nil, "Select a method:", tUserChoices )
        if sOpt == nil or sOpt == "Cancel Operation" then
            print( "Aborting..." )
            return
        end

        executeMenuAction( sOpt, proxy.m[sOpt] )
    end

    proxy = nil

end -- menuConnectService

----------------------------------------------------------------
--- menuListAdapters
---
--- [add description here]
---
----------------------------------------------------------------
local function menuListAdapters()

    --[[ Example:
    dbus-send --system --dest=org.bluez --print-reply / org.bluez.Manager.ListAdapters
    dbus-send --system --dest=org.bluez --print-reply / org.bluez.Manager.DefaultAdapter
    ]]
    executeMenuAction( "ListAdapters",   gMgrProxy.m.ListAdapters )
    print( "" )
    executeMenuAction( "DefaultAdapter", gMgrProxy.m.DefaultAdapter )

    -- Override the last command
    gLastFunct = menuListAdapters
    gLastArgs  = nil
    gLastInfo  = "List Adapters"

end -- menuListAdapters

----------------------------------------------------------------
--- menuListDevices
---
--- [add description here]
---
----------------------------------------------------------------
local function menuListDevices()

    local proxy = getAdapterProxy()
    if proxy == nil then
        return
    end

    --[[ Example:
    dbus-send --system --dest=org.bluez --print-reply /org/bluez/971/hci0 org.bluez.Adapter.ListDevices
    ]]
    executeMenuAction( "ListDevices", proxy.m.ListDevices )

    print( "\nDiscovery List:" )
    for _,tInfo in pairs(gDiscoveryList) do

        print( string.format( "%s  %10s %7s  %8s [%s]",
                              tInfo.Address,
                              tInfo.Name or tInfo.Alias or "[unknown]",
                              ( tInfo.Paired==true ) and "paired" or "~paired",
                              ( tInfo.Trusted==true ) and "trusted" or "~trusted",
                              tInfo.Icon
                              ) )
    end
    print( "" )

    proxy = nil

end -- menuListDevices

----------------------------------------------------------------
--- menuStartStopDiscovery
---
--- [add description here]
---
----------------------------------------------------------------
local function menuStartStopDiscovery()

    local tDiscovering = {}

    gSilentExecute = true
    local status, tAdapters = executeMenuAction( "ListAdapters", gMgrProxy.m.ListAdapters )

    if status == false then
        print( "ERROR: Unable to get the adapter list." )
        return nil
    end

    if #tAdapters == 0 then
        print( "INFO: No adapters detected." )
        return nil
    end

    local tAdapterListStr = {}
    for _,adapterPath in pairs( tAdapters ) do

        local proxyCtrl = proxyctrl.new(gSystemConn, BLUEZ_ADAPTER[BUSNAME], adapterPath )
        assert(proxyCtrl:bind())

        local proxy = proxyCtrl:getProxy( BLUEZ_ADAPTER[IFACE] )
        assert( nil ~= proxy )

        gSilentExecute = true
        local status, result = executeMenuAction( "GetProperties", proxy.m.GetProperties )

        if status then
            local discovering = nil
            if result.Discovering ~= nil then
                discovering = result.Discovering
                table.insert( tAdapterListStr,
                              string.format( "%s - %s",
                                             adapterPath,
                                             (result.Discovering==true) and "discovering" or "~discovering" ))
            else
                table.insert( tAdapterListStr,
                              string.format( "%s - [error]", adapterPath ))
            end
            tDiscovering[ tostring( #tAdapterListStr ) ] = { path      = adapterPath,
                                                             proxy     = proxy,
                                                             proxyCtrl = proxyCtrl,
                                                             disc      = discovering }
        end

    end -- adapter listing

    local adapterIdx = 1
    if #tAdapterListStr > 1 then
        adapterIdx = gPrompter:selection( nil,
                                          "Select an adapter to start/stop:",
                                          tAdapterListStr, true )
        if adapterIdx == nil or tDiscovering[ tostring( adapterIdx ) ] == nil then
            print( "Aborting..." )
            return nil
        end
    else
        print( "\nSelected: ", tAdapterListStr[1], "\n" )
    end

    adapterIdx = tostring( adapterIdx )

    local op = gPrompter:selection( (tDiscovering[ adapterIdx ].disc) and "stop" or "start",
                                    "Select the operation:",
                                    { "start", "stop" } )
    if op == nil then
        print( "Aborting..." )
        return nil
    end

    if op == "stop" then
        -- Already discovering, so stop
        executeMenuAction( "StopDiscovery", tDiscovering[ adapterIdx ].proxy.m.StopDiscovery )
    else
        -- Not discovering, so start
        if tDiscovering[ adapterIdx ].disc == nil then
            print( "ERROR: Discovering state unknown, but we will attempt to start." )
        end

        -- Track the changes that are signalled
        tDiscovering[ adapterIdx ].proxyCtrl:connectSignal( BLUEZ_ADAPTER[IFACE],
                                                            "PropertyChanged",
                        function ( sProperty, val )
                            if type(val) == "table" then
                                 print( "\nPropertyChanged: ", sProperty, pretty.write(val) )
                             else
                                 print( "\nPropertyChanged: ", sProperty, val )
                             end

                            -- This will cycle on/off every so many seconds
                            if sProperty == "Discovering" then

                                if val == true then
                                    -- Starting discovery

                                    -- TODO: May want to age the entries
                                    --       in gDiscoveryList.
                                else
                                    -- Stopping discovery

                                    -- TODO: May want to age the entries
                                    --       in gDiscoveryList.
                                end

                            end

                        end )

        tDiscovering[ adapterIdx ].proxyCtrl:connectSignal( BLUEZ_ADAPTER[IFACE],
                                                            "DeviceFound",
                        function ( sMacAddr, tInfo )
                            print( "\nDeviceFound: ", sMacAddr )
                            pretty.dump( tInfo )

                            --[[
                            DeviceFound:    D8:D1:CB:A8:AA:13
                            {
                              LegacyPairing = false,
                              Paired = false,
                              Class = 7995916,
                              Trusted = false,
                              Name = "tPhone",
                              Address = "D8:D1:CB:A8:AA:13",
                              RSSI = -65,
                              UUIDs = {
                                "00000000-deca-fade-deca-deafdecacafe",
                                "00001132-0000-1000-8000-00805f9b34fb",
                                "00001116-0000-1000-8000-00805f9b34fb",
                                "0000110c-0000-1000-8000-00805f9b34fb",
                                "0000110a-0000-1000-8000-00805f9b34fb",
                                "0000112f-0000-1000-8000-00805f9b34fb",
                                "0000111f-0000-1000-8000-00805f9b34fb",
                                "00001200-0000-1000-8000-00805f9b34fb"
                              },
                              Alias = "tPhone",
                              Icon = "phone"
                            }
                            ]]

                            gDiscoveryList[ sMacAddr ] = tInfo

                            -- In case we need it later
                            gDiscoveryList[ sMacAddr ].AdapterPath = tDiscovering[ adapterIdx ].path
                        end )

        tDiscovering[ adapterIdx ].proxyCtrl:connectSignal( BLUEZ_ADAPTER[IFACE],
                                                            "DeviceDisappeared",
                        function ( sMacAddr )
                            print( "\nDeviceDisappeared: ", sMacAddr )
                            gDiscoveryList[ sMacAddr ] = nil
                        end )

        --[[ Example:
        dbus-send --system --dest=org.bluez --print-reply /org/bluez/971/hci0 org.bluez.Adapter.StartDiscovery
        ]]
        executeMenuAction( "StartDiscovery", tDiscovering[ adapterIdx ].proxy.m.StartDiscovery )
    end

end -- menuStartStopDiscovery

local function menuOptionsAdapters()

    local tDeviceStrList = {}

    -- - - - - - - - - - - - - - - - - - - - - - - - - -
    local function getDevice()

        local devStr = ""

        if #tDeviceStrList > 1 then

            devStr = gPrompter:selection( nil,
                                          "Select a device:",
                                          tDeviceStrList )
            if devStr == nil then
                print( "Aborting..." )
                return nil
            end

        elseif #tDeviceStrList == 1 then

            devStr = tDeviceStrList[1]
            print( "Using Device: ",  devStr.."\n" )
        end

        return gDiscoveryList[ string.match( devStr, "^(%S*).*" ) ]

    end -- getDevice
    -- - - - - - - - - - - - - - - - - - - - - - - - - -

    local proxy, proxyCtrl = getAdapterProxy()
    if proxy == nil then
        return
    end

    -- Track the changes that are signalled
    local hSig =proxyCtrl:connectSignal( BLUEZ_ADAPTER[IFACE],
                             "PropertyChanged",
                             function ( sProperty, val )
                                 if type(val) == "table" then
                                     print( "\nPropertyChanged: ", sProperty, pretty.write(val) )
                                 else
                                     print( "\nPropertyChanged: ", sProperty, val )
                                 end
                             end )

    while true do

        tDeviceStrList = {}

        gSilentExecute = true
        local status, tDeviceList = executeMenuAction( "ListDevices", proxy.m.ListDevices )
        if tDeviceList == nil then
            tDeviceList = {}
        end

        -- Make sure these are contianed in gDiscoveryList
        for _,sDevicePath in pairs(tDeviceList) do
            local sMacAddr = string.match( sDevicePath, "/dev_(%S*)" )
            sMacAddr = string.gsub( sMacAddr, "_", ":" )

            if gDiscoveryList[sMacAddr] then
                gDiscoveryList[sMacAddr].CreatedPath = sDevicePath
            else
                gDiscoveryList[sMacAddr] = { CreatedPath = sDevicePath,
                                             Address     = sMacAddr }
            end
        end -- loop

        -- Make a presentable list of info for the user
        for _,tInfo in pairs(gDiscoveryList) do
            table.insert( tDeviceStrList,
                          string.format( "%s  %12s %7s %8s %8s [%s]",
                                          tInfo.Address,
                                          tInfo.Name or tInfo.Alias or "[unknown]",
                                          ( tInfo.Paired==true ) and "paired" or "~paired",
                                          ( tInfo.Trusted==true ) and "trusted" or "~trusted",
                                          ( tInfo.CreatedPath ) and "created" or "~created",
                                          tInfo.Icon or "?" ) )
        end -- loop

        -- Submenu
        -----------------------------------------------------------
        local tMenu =  { "Info/Properties",
                         "CreateDevice() (SDP Records Only)",
                         "CreatePairedDevice() (SDP Records AND Initiate Pairing)",
                         "RemoveDevice()",
                         "FindDevice()",
                         "Power On/Off",
                         "ListAdapters()",
                         "Start/stop Discovery",
                         "Toggle Discoverable Setting",
                         "Toggle Pairable Setting",
                         "Exit to Main Menu" }

        if gLastFunct ~= nil then
            table.insert( tMenu, #tMenu, "Execute Last Command == "..(gLastInfo or "") )
        end

        print( "" )
        local opt = gPrompter:selection( nil, "Select an option:", tMenu, true )
        if opt == nil then
            print( "Aborting..." )
            break
        end

        if opt == #tMenu then -- "Exit to Main Menu"
            break
        end

        -- Item Processing
        -----------------------------------------------------------

        if opt == 1 then      -- "Info/Properties"

            --[[ Example:
                dbus-send --system --dest=org.bluez --print-reply /org/bluez/971/hci0 org.bluez.Adapter.GetProperties
              ]]
            executeMenuAction( "GetProperties", proxy.m.GetProperties )

        elseif opt == 2 then  -- "CreateDevice (SDP Records Only)"

            local tDevice = getDevice()
            if tDevice == nil then
                break
            end

            gConfirmExecute = true
            local status, tDevObjPath = executeMenuAction( "CreateDevice", proxy.m.CreateDevice, tDevice.Address )
            if tDevObjPath then
                tDevice.CreatedPath = tDevObjPath
            end

        elseif opt == 3 then  -- "CreateDevice (SDP Records AND Initiate Pairing)"

            local tDevice = getDevice()
            if tDevice == nil then
                break
            end

            gConfirmExecute = true
            local status, tDevObj = executeMenuAction( "CreatePairedDevice",
                                                       proxy.m.CreatePairedDevice,
                                                       tDevice.Address,
                                                       l2dbus.DbusTypes.ObjectPath.new (PAIRING_AGENT[OBJPATH]), -- not necessary, can pass just string, here as example
                                                       "DisplayYesNo" )

        elseif opt == 4 then  -- "RemoveDevice"

            local tDevice = getDevice()
            if tDevice == nil then
                break
            end

            gConfirmExecute = true
            executeMenuAction( "RemoveDevice", proxy.m.RemoveDevice, tDevice.CreatedPath )

        elseif opt == 5 then  -- "FindDevice"

            local tDevice = getDevice()
            if tDevice == nil then
                break
            end

            executeMenuAction( "FindDevice", proxy.m.FindDevice, tDevice.Address )

        elseif opt == 6 then  -- "Power On/Off"

            local tMenu =  { "On",
                             "Off",
                             "Cancel operation" }
            local opt = gPrompter:selection( nil, "Select an option:", tMenu, true )
            if opt == nil then
                print( "Aborting..." )
                break
            end

            if opt ~= #tMenu then -- "NOT Cancel operation"

                gConfirmExecute = true
                executeMenuAction( "SetProperty", proxy.m.SetProperty,
                                   "Powered", l2dbus.DbusTypes.Variant.new((opt == 1), "vb") )

            end

        elseif opt == 7 then  -- "List Adapters"

            menuListAdapters()

        elseif opt == 8 then -- "Start/stop Discovery"

            menuStartStopDiscovery( )

        elseif opt == 9 then -- "Toggle Discoverable Setting"

            local tMenu =  { "Discoverable",
                             "Hidden",
                             "Cancel Operation" }
            local opt = gPrompter:selection( nil, "Select an option:", tMenu, true )
            if opt == nil then
                print( "Aborting..." )
                break
            end

            if opt ~= #tMenu then -- "NOT Cancel Operation"

                executeMenuAction( "SetProperty", proxy.m.SetProperty,
                                   "Discoverable", l2dbus.DbusTypes.Variant.new((opt == 1), "vb") )
            end

        elseif opt == 10 then -- "Toggle Pairable Setting"

            local tMenu =  { "Pairable",
                             "Non-Pairable",
                             "Cancel Operation" }
            local opt = gPrompter:selection( nil, "Select an option:", tMenu, true )
            if opt == nil then
                print( "Aborting..." )
                break
            end

            if opt ~= #tMenu then -- "NOT Cancel Operation"

                executeMenuAction( "SetProperty", proxy.m.SetProperty,
                                   "Pairable", l2dbus.DbusTypes.Variant.new((opt == 1), "vb") )
            end

        elseif gLastFunct and opt == #tMenu-1 then  -- "Last Comand"

            if gLastArgs then
                gLastFunct( unpack( gLastArgs ) )
            else
                gLastFunct( )
            end

        end

    end -- infinite menu loop

    proxyCtrl:disconnectSignal( hSig )

end -- menuOptionsAdapters

local function menuOptionsDevices()

    local hSig

    while true do

        -- Submenu
        -----------------------------------------------------------
        local tMenu = { "Get Properties/Info",
                        "DiscoverServices()",
                        "CancelDiscover() (cancel DiscoverServices())",
                        "Disconnect()",
                        "Toggle Trusted Setting for this Device",
                        "Connect to Service (PAN, HFP, Audio, etc)",
                        "Exit to Main Menu" }

        if gLastFunct ~= nil then
            table.insert( tMenu, #tMenu, "Execute Last Command == "..(gLastInfo or "") )
        end

        print( "" )
        local opt = gPrompter:selection( {}, "Select an option:", tMenu, true )
        if opt == nil then
            print( "Aborting..." )
            break
        end

        if opt == #tMenu then -- "Exit to Main Menu"
            break
        end

        -- Get the proxy for the control and device interfaces for the device
        local ctrlProxy, adapterProxy, proxyCtrl = getDeviceIfaceProxy( BLUEZ_CTRL )
        if ctrlProxy == nil or adapterProxy == nil or proxyCtrl == nil then
            print( "Aborting..." )
            return nil
        end
        local devProxy = getDeviceIfaceProxy( BLUEZ_DEVICE, adapterProxy, true )

        -- Track the changes that are signalled
        hSig = proxyCtrl:connectSignal( BLUEZ_ADAPTER[IFACE],
                             "PropertyChanged",
                             function ( sProperty, val )
                                 if type(val) == "table" then
                                     print( "\nPropertyChanged: ", sProperty, pretty.write(val) )
                                 else
                                     print( "\nPropertyChanged: ", sProperty, val )
                                 end
                             end )

        -- Item Processing
        -----------------------------------------------------------

        if opt == 1 then      -- "Get Properties/Info"

            executeMenuAction( "control.GetProperties", ctrlProxy.m.GetProperties )
            print( "" )
            local status, tProp = executeMenuAction( "device.GetProperties", devProxy.m.GetProperties )
            if status and tProp and tProp.Address then
                -- Update our stored info
                local sMacAddr = string.match( gLastDevice or "", "/dev_(%S*)" )
                sMacAddr = string.gsub( sMacAddr, "_", ":" )
                tProp.CreatedPath        = gLastDevice
                gDiscoveryList[sMacAddr] = tProp
            end

        elseif opt == 2 then  -- "DiscoverServices()"

            local status, tXml = executeMenuAction( "DiscoverServices", devProxy.m.DiscoverServices, "" )
            -- This returns an array of key to XML object pairs
            if status and tXml then
                local tRecords = {}
                -- Simplifies to the XML a little for easier viewing--not perfect in anyway
                for k,v in pairs(tXml) do
                    local tRec = {}
                    string.gsub( v, '<attribute id="(%P*)">%s(.-)</attribute>',
                                 function( attr,val )
                                     -- Count sequences and do some minor improvements
                                     local count
                                     _,count = string.gsub( val, "<sequence>%s", "" )
                                     if count == 1 then
                                         val = string.gsub( val, "<sequence>%s", "" )
                                         val = string.gsub( val, "</sequence>%s", "" )
                                         --val = stringx.strip( val )
                                     else

                                     end

                                     tRec[attr] = val
                                 end )
                    tRecords[k] = tRec
                end
                print( string.rep( "=", 60 ) )
                pretty.dump( tRecords )
                print( string.rep( "=", 60 ) )
            end

        elseif opt == 3 then  -- "CancelDiscover() (cancel DiscoverServices())"

            executeMenuAction( "CancelDiscovery", devProxy.m.CancelDiscovery )

        elseif opt == 4 then  -- "Disconnect"

            gConfirmExecute = true
            executeMenuAction( "Disconnect", devProxy.m.Disconnect )

        elseif opt == 5 then  -- "Toggle Trusted Setting for this Device"

            local tMenu =  { "Trusted",
                             "NOT Trusted",
                             "Cancel Operation" }
            local opt = gPrompter:selection( nil, "Select an option:", tMenu, true )
            if opt == nil then
                print( "Aborting..." )
                break
            end

            if opt ~= #tMenu then -- "NOT Cancel Operation"

                executeMenuAction( "SetProperty", devProxy.m.SetProperty,
                                   "Trusted", l2dbus.DbusTypes.Variant.new((opt == 1), "vb") )
            end

        elseif opt == 6 then  -- "Connect to Service (PAN, HFP, etc)"

            menuConnectService( proxyCtrl )

        elseif gLastFunct and opt == #tMenu-1 then  -- "Last Comand"

            if gLastArgs then
                gLastFunct( unpack( gLastArgs ) )
            else
                gLastFunct( )
            end

        end

        ctrlProxy    = nil
        adapterProxy = nil
        devProxy     = nil

    end -- infinite menu loop

end -- menuOptionsDevices

----------------------------------------------------------------
--- menuMain
---
--- [add description here]
---
----------------------------------------------------------------
local function menuMain()

    local doExit = false
    while not doExit do
        print("\nLua D-Bus BlueZ Test Client v"..APP_VER)
        print(string.rep("=", 50))
        print()
        print("a.   Adapter menu options (list, discovery, pairing, etc)")
        print("d.   Device menu options (info, connect service, etc)")

        --print("m.   Manage adapter (Device SDP records, Pairing, etc)")
        --print("n.   manage devices (IsConnected, Info, etc)")
        --print("s.   Start/stop discovery")
        if gLastFunct then
            print("z.   execute the last command", "== "..gLastInfo or "" )
        end
        print("q.   Quit")
        print(string.rep("=", 50))

        io.stdout:write("\nEnter a command: ")
        local cmd = gPrompter:getChar()
        print("\n")
        if cmd == 'q' then
            doExit = true
        elseif cmd == 'a' then
            menuOptionsAdapters()
        elseif cmd == 'd' then
            menuOptionsDevices()
        elseif gLastFunct and cmd == 'z' then
            if gLastArgs then
                gLastFunct( unpack( gLastArgs ) )
            else
                gLastFunct( )
            end
        end
    end

    gDispatcher:stop()

end -- menuMain


----------------------------------------------------------------
--- popup
---
--- Generic popup routine
---
--- @tparam   (string)  sPrompt ....
--- @tparam   (string)  sTitle  ....if nil, "Pairing Prompt"
--- @tparam   (string)  sType   ...."YorN", Yes/No prompt
--- @tparam   (string)  sType   ...."prompt", line of text for input
---                                  nil, just display prompt, no input needed
---
--- @treturn  (string)  "YorN", returns "Y" or "N"
----------------------------------------------------------------
local function popup( sPrompt, sTitle, sType )

    ----------------------------------------------------------------
    --- split
    ---
    --- split a string based on a specific character.            <br>
    ---                                                          <br>
    --- Code taken from:
    --- <a href="http://snippets.luacode.org/?p=snippets/Split_a_string_into_a_list_5">Here</a> <br>
    ---                                                          <br>
    --- LICENSE: MIT/X11                                         <br>
    ---                                                          <br>
    --- @tparam   (string)  s  ....string to split
    --- @tparam   (string)  re ....(regular expression) delimiter to split (if nil, space)
    ---
    --- @treturn  (table) table of values split by "re"
    ----------------------------------------------------------------
    local function split(s,re)
        local i1 = 1
        local ls = {}
        local append = table.insert
        if not re then
            re = '%s+'
        end
        if re == '' then
            return {s}
        end
        while true do                                                            --> lcov: ref+1
            local i2,i3 = s:find(re,i1)
            if not i2 then
                local last = s:sub(i1)
                if last ~= '' then
                    append(ls,last)
                end
                if #ls == 1 and ls[1] == '' then
                    return {}
                else
                    return ls
                end
            end
            append(ls,s:sub(i1,i2-1))
            i1 = i3+1
        end                                                                      --> lcov: ref-1

    end -- split

    sTitle = sTitle or "Pairing Prompt"
    sTitle = sTitle.." |"

    print( "" )
    print( string.rep( "*", 60 ) )
    print( string.format( "* %-56s *", sTitle ) )
    print( string.format( "* %-56s *", string.rep("-", #sTitle) ) )
    print( string.format( "* %-56s *", "" ) )
    local sLines = split( sPrompt, "\n" )
    for _, sLine in pairs(sLines) do
        print( string.format( "* %-56s *", sLine ) )
    end
    print( string.rep( "*", 60 ) )

    local cmd = ""
    if sType == "YorN" then
        while cmd ~= "Y" and cmd ~= "N" and
              cmd ~= "y" and cmd ~= "n" do
            io.stdout:write("(Y)es or (N)o? ")
            cmd = io.stdin:read( 1 )
            print( "\n" )
        end
        cmd = string.upper(cmd)

    elseif sType == "prompt" then
        cmd = io.stdin:read( "*line" )
        print( "\n" )
    end
    return cmd

end -- popup



----------------------------------------------------------------
--- hfp_NewConnection
---
--- void NewConnection(filedescriptor fd, uint16 version)
---
--- This method gets called whenever a new handsfree
--- connection has been established.  The objectpath
--- contains the object path of the remote device.  This
--- method assumes that DBus daemon with file descriptor
--- passing capability is being used.
---
--- The agent should only return successfully once the
--- establishment of the service level connection (SLC)
--- has been completed.  In the case of Handsfree this
--- means that BRSF exchange has been performed and
--- necessary initialization has been done.
---
--- Possible Errors: org.bluez.Error.InvalidArguments
---          org.bluez.Error.Failed
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gHfpMethods.hfp_NewConnection( ifaceObj, conn, msg, userdata )

    print( "TODO: Implement NewConnection() (returns: Failed)" )
    pretty.dump( msg:getArgsAsArray() )

    conn:send( l2dbus.Message.newError( msg, "org.bluez.Error.Failed", "" ) )

end -- hfp_NewConnection

----------------------------------------------------------------
--- hfp_NewConnection
---
--- void Release()
---
--- This method gets called whenever the service daemon
--- unregisters the agent or whenever the Adapter where
--- the HandsfreeAgent registers itself is removed.
---
--- @tparam   (string)  ifaceObj  ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gHfpMethods.hfp_Release( ifaceObj, conn, msg, userdata )

    print( "TODO: Implement Release() (returns: Failed)" )
    pretty.dump( msg:getArgsAsArray() )

    conn:send( l2dbus.Message.newError( msg, "org.bluez.Error.Failed", "" ) )

end -- hfp_Release


----------------------------------------------------------------
--- pairing_Authorize
---
--- void Authorize(object device, string uuid)
---
--- This method gets called when the service daemon
--- needs to authorize a connection/service request.
---
--- Possible errors: org.bluez.Error.Rejected
---                  org.bluez.Error.Canceled
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gPairMethods.pairing_Authorize( ifaceObj, conn, msg, userdata )

    local tResults = msg:getArgsAsArray()

    local ret = popup( string.format("Authorize?\ndevice:%s\nuuid: %s",
                                     tResults[1] or "", tResults[2] or "" ),
                       "Authorization Request", "YorN")

    if ret == "Y" then
        print( "Authorized\n" )
        conn:send( l2dbus.Message.newMethodReturn( msg ) )
    else
        print( "Rejected\n" )
        conn:send( l2dbus.Message.newError( msg, "org.bluez.Error.Rejected", "" ) )
    end

end -- pairing_Authorize

----------------------------------------------------------------
--- pairing_Cancel
---
--- void Cancel()
---
--- This method gets called to indicate that the agent
--- request failed before a reply was returned.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gPairMethods.pairing_Cancel( ifaceObj, conn, msg, userdata )

    popup( "Agent Cancelled", "Agent API" )
    -- Used to cancel the displaying of the passkey (N/A here)
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- pairing_Cancel

----------------------------------------------------------------
--- pairing_ConfirmModeChange
---
--- void ConfirmModeChange(string mode)
---
--- This method gets called if a mode change is requested
--- that needs to be confirmed by the user. An example
--- would be leaving flight mode.
---
--- Possible errors: org.bluez.Error.Rejected
---                  org.bluez.Error.Canceled
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gPairMethods.pairing_ConfirmModeChange( ifaceObj, conn, msg, userdata )

    local tResults = msg:getArgsAsArray()

    local ret = popup( tResults[1] or "", "Confirm Mode Change", "YorN")

    if ret == "Y" then
        print( "Allowed\n" )
        conn:send( l2dbus.Message.newMethodReturn( msg ) )
    else
        print( "Rejected\n" )
        conn:send( l2dbus.Message.newError( msg, "org.bluez.Error.Rejected", "" ) )
    end

end -- pairing_ConfirmModeChange

----------------------------------------------------------------
--- pairing_DisplayPasskey
---
--- void DisplayPasskey(object device, uint32 passkey, uint8 entered)
---
--- This method gets called when the service daemon
--- needs to display a passkey for an authentication.
---
--- The entered parameter indicates the number of already
--- typed keys on the remote side.
---
--- An empty reply should be returned. When the passkey
--- needs no longer to be displayed, the Cancel method
--- of the agent will be called.
---
--- During the pairing process this method might be
--- called multiple times to update the entered value.
---
--- Note that the passkey will always be a 6-digit number,
--- so the display should be zero-padded at the start if
--- the value contains less than 6 digits.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gPairMethods.pairing_DisplayPasskey( ifaceObj, conn, msg, userdata )

    local tResults = msg:getArgsAsArray()

    popup( string.format("Remote User Enter: %06d (%d digits entered)",
                         tResults[2] or 0, tResults[3] or 0 ),
           "Remote Passkey" )

    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- pairing_DisplayPasskey

----------------------------------------------------------------
--- pairing_DisplayPinCode
---
--- This method gets called when the service daemon
--- needs to display a pincode for an authentication.
---
--- An empty reply should be returned. When the pincode
--- needs no longer to be displayed, the Cancel method
--- of the agent will be called.
---
--- If this method is not implemented the RequestPinCode
--- method will be used instead.
---
--- This is used during the pairing process of keyboards
--- that don't support Bluetooth 2.1 Secure Simple Pairing,
--- in contrast to DisplayPasskey which is used for those
--- that do.
---
--- This method will only ever be called once since
--- older keyboards do not support typing notification.
---
--- Note that the PIN will always be a 6-digit number,
--- zero-padded to 6 digits. This is for harmony with
--- the later specification.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gPairMethods.pairing_DisplayPinCode( ifaceObj, conn, msg, userdata )

    local tResults = msg:getArgsAsArray()

    popup( string.format("Pin Code: %06d",
                         tonumber(tResults[2]) or 0 ),
           "PinCode" )

    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- pairing_DisplayPinCode

----------------------------------------------------------------
--- pairing_Release
---
--- void Release()
---
--- This method gets called when the service daemon
--- unregisters the agent. An agent can use it to do
--- cleanup tasks. There is no need to unregister the
--- agent, because when this method gets called it has
--- already been unregistered.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gPairMethods.pairing_Release( ifaceObj, conn, msg, userdata )

    popup( "Agent Released", "Agent API" )
    -- NOP
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- pairing_DisplayRelease

----------------------------------------------------------------
--- pairing_RequestConfirmation
---
--- void RequestConfirmation(object device, uint32 passkey)
---
--- This method gets called when the service daemon
--- needs to confirm a passkey for an authentication.
---
--- To confirm the value it should return an empty reply
--- or an error in case the passkey is invalid.
---
--- Note that the passkey will always be a 6-digit number,
--- so the display should be zero-padded at the start if
--- the value contains less than 6 digits.
---
--- Possible errors: org.bluez.Error.Rejected
---                  org.bluez.Error.Canceled
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gPairMethods.pairing_RequestConfirmation( ifaceObj, conn, msg, userdata )

    local tResults = msg:getArgsAsArray()

    -- Since we are using a static passkey, will not get user input
    local cmd = popup( string.format("Authorize?\ndevice:         %s\nremote passkey: %06d\n",
                                     tResults[1] or "", tResults[2] or 0 ),
                       "Request Confirmation", "YorN" )

    if cmd == "Y" then
        conn:send( l2dbus.Message.newMethodReturn( msg ) )
    else
        conn:send( l2dbus.Message.newError( msg, "org.bluez.Error.Rejected", "" ) )
    end

end -- pairing_RequestConfirmation

----------------------------------------------------------------
--- pairing_RequestPasskey
---
--- uint32 RequestPasskey(object device)
---
--- This method gets called when the service daemon
--- needs to get the passkey for an authentication.
---
--- The return value should be a numeric value
--- between 0-999999.
---
--- Possible errors: org.bluez.Error.Rejected
---                  org.bluez.Error.Canceled
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gPairMethods.pairing_RequestPasskey( ifaceObj, conn, msg, userdata )

    local passkey  = DEFAULT_PASSKEY
    local tResults = msg:getArgsAsArray()

    -- Uses hard-coded passkey, if you wanted you could:
    -- + Randomly generate it
    -- + Prompt the user
    popup( string.format("Device:        %s\nUsing Passkey: %06d",
                         tResults[1] or "", passkey),
           "Passkey Request" )

    local replyMsg = l2dbus.Message.newMethodReturn( msg )
    replyMsg:addArgs( passkey )

    conn:send( replyMsg )

end -- pairing_RequestPasskey

----------------------------------------------------------------
--- pairing_RequestPinCode
---
--- string RequestPinCode(object device)
---
--- This method gets called when the service daemon
--- needs to get the passkey for an authentication.
---
--- The return value should be a string of 1-16 characters
--- length. The string can be alphanumeric.
---
--- Possible errors: org.bluez.Error.Rejected
---                  org.bluez.Error.Canceled
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gPairMethods.pairing_RequestPinCode( ifaceObj, conn, msg, userdata )

    local pinCode  = DEFAULT_PINCODE
    local tResults = msg:getArgsAsArray()

    -- Uses hard-coded pin code, if you wanted you could:
    -- + Randomly generate it
    -- + Prompt the user
    popup( string.format("Device:         %s\nUsing Pin Code: %06d",
                         tResults[1] or "", pinCode),
           "Pin Code Request" )

    local replyMsg = l2dbus.Message.newMethodReturn( msg )
    replyMsg:addArgs( pinCode )

    conn:send( replyMsg )

end -- pairing_RequestPinCode


----------------------------------------------------------------
--- main
---
--- Start the main loop and start the menu on a coroutine
----------------------------------------------------------------
local function main()

    initDbus()

    local timer = l2dbus.Timeout.new(gDispatcher, 25, false, function()
                        local co = coroutine.create(menuMain)
                        local status, result = coroutine.resume(co)
                        if not status then
                            print("Error: " .. result)
                        end
                  end)
    timer:setEnable(true)

    gDispatcher:run(l2dbus.Dispatcher.DISPATCH_WAIT)

    gProxyCtrl:disconnectAllSignals()
    gPrompter:restoreTtyState()

    gPrompter       = nil
    gMgrProxy       = nil
    gProxyCtrl      = nil
    gDispatcher     = nil

end -- main

main()


l2dbus.shutdown()


