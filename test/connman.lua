#!/usr/bin/env lua
----------------------------------------------------------------
--- connman Tester
---
--- This is a simple client for exercising parts of connman.  This
--- code is meant to exercise various parts of the l2dbus API
--- and prove the code works with other 3rd party services. This
--- tool is NOT meant to be an exhaustive connman tool.
---
--- The current implementation supports:
--- + Technology, Manager, Service, and Clock API's available
--- + Main and Counter agents implemented
---
--- NOTE: Tested with connamn v1.19+
---
--- Some Limitations:
--- =============================
--- 1. Have seen where the IPv4 config changes continually.
---    Service PropertyChanged: 	IPv4	{
---      Netmask = "255.255.255.0",
---      Gateway = "192.168.120.1",
---      Method = "dhcp",
---      Address = "192.168.120.123"
---    }
---
---    Service PropertyChanged: 	IPv4	{
---      Netmask = "255.255.255.0",
---      Method = "dhcp",
---      Address = "192.168.120.123"
---    }
---    (over and over)
---    You will see obviously see increased CPU usage for connmand
---    as well as the DBus daemon.
---    Root Cause: unknown
---    Workaround: reboot and it often goes away.
---
--- 2. Currently did not implement VPN support
--- 3. Currently did not implement WPS support
---
---
--- System Setup:
--- =============================
--- This MUST run on the dbus system bus, therefore you must open
--- access for this service.  For example, add the following file(s):
--- /etc/dbus-1/system.d/com.service.TestConnMan.conf
---
--- <!-- This configuration file specifies the required security policies
---      for connmand tester daemon to work. -->
---
--- <!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
---  "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
--- <busconfig>
---
---   <!-- ../system.conf have denied everything, so we just punch some holes -->
---
---   <policy user="root">
---     <allow own="com.service.TestConnMan"/>
---     <allow send_destination="com.service.TestConnMan"/>
---     <allow send_interface="net.connman.Manager"/>
---     <allow send_interface="net.connman.Technology"/>
---     <allow send_interface="net.connman.Clock"/>
---     <allow send_interface="net.connman.Service"/>
---   </policy>
---
---   <policy at_console="true">
---     <allow send_destination="com.service.TestConnMan"/>
---   </policy>
---
---   <policy context="default">
---     <allow send_destination="com.service.TestConnMan"/>
---   </policy>
---
--- </busconfig>
---
---
--- Simple Testing Procedures:
--- =============================
---
--- ==================
---     Ethernet
--- ==================
---
--- Set Static IP Address:
--- ------------------------
--- 1. option 's' (Service Submenu)
--- 2. option 's' (SetProperties)
--- 3. Select the "Wired" Ethernet connection
--- 4. option '2' (IPv4.Configuration)
--- 5. Select 'manual', then set the address
---
---
--- ==================
---       WiFi
--- ==================
---
--- Wifi Initial Scan:
--- ------------------------
--- 1. Make sure WiFi is powered on, to make sure:
---    a) option 't' (Technologies Submenu...)
---       i) The listing will show if WiFi is powered on, if not:
---          a) option 's' (SetProperties)
---          b) select Wifi, then follow the menus to enable power
--- NOTE: When powered on it will create a service for each AP found.
--- NOTE: When in doubt, best start the scan manually (see Wifi Further Scan)
---
--- Wifi Further Scan:
--- ------------------------
--- 1. Make sure WiFi is powered on, if not follow, "Wifi Initial Scan"
--- 2. option 't' (Technologies Submenu...)
--- 3. option 'c' (scan)
--- 4. select WiFi, this will not return until the scan is complete.
---    It may be best to ignore the response since it will add services
---    for each AP found.
---
--- Wifi Connect:
--- ------------------------
--- 1. Make sure WiFi is powered on, if not follow, "Wifi Initial Scan"
--- 2. option 's' (Service Submenu)
--- 3. option 'c' (Connect)
---    Select the AP you want to select to.  You will then be prompted for
---    the appropriate information (RequestInput).  Once the correct data
---    has been entered it is ready for use.
---
--- View Available Wifi AP:
--- ------------------------
--- (see "Misc.View Connections" below)
---
--- Auto Connect Wifi at Startup:
--- ------------------------------
--- After the AP is visible by the connman you can enable the AutoConnect
--- feature by enabling this property.
--- NOTE: This will NOT auto connect if Wired Ethernet is connected.
--- 1. option 's' (Service Submenu)
--- 2. option 's' (SetProperties)
--- 3. Select the WiFi AP/Service
--- 4. Select "AutoConnect" and enable the option.
---
---
--- ==================
---     Bluetooth
--- ==================
---
--- BT Discovery:
--- ------------------------
--- Must use Bluez
---
--- BT Pairing:
--- ------------------------
--- Must use Bluez
---
--- BT PAN:
--- ------------------------
--- 1. Make sure Bluetooth is powered on, if not follow, "Wifi Initial Scan"
--- 2. Use Bluez to pair the phone (Phone MUST obviously have PAN support)
--- 3. option 's' (Service Submenu)
--- 4. option 'c' (Connect), select the phone with PAN
--- You will soon get an IP address and your done.
---
--- ==================
---     Misc
--- ==================
---
--- View Connections:
--- ------------------------
--- 1. option 'm' (Manager Submenu)
--- 2. option 'v' (GetServices)
--  OR
--- 1. option 's' (Service Submenu)
--- 2. option 'g' (GetProperties...)
--- (both options use the same Manager API)
--- NOTE: It will list connected and available connections
---
--- Know When a Connection Drops:
--- ------------------------------
--- 1.
---
----------------------------------------------------------------
local proxyctrl = require("l2dbus.proxyctrl")
local l2dbus    = require("l2dbus")
local lapp      = require("pl.lapp")
local pretty    = require("pl.pretty")
local tablex    = require("pl.tablex")
local utils     = require("pl.utils")
local Prompter  = require("utils.prompter")
local posix     = require( "posix" )

-- Const
--------
local APP_VER       = "1.1.0"

local BUSNAME         = 1
local OBJPATH         = 2
local IFACE           = 3
local CONNMAN_MGR     = {"net.connman", "/", "net.connman.Manager"}
local CONNMAN_CLOCK   = {"net.connman", "/", "net.connman.Clock"}
local CONNMAN_SESSION = {"net.connman", nil, "net.connman.Session"}
local CONNMAN_SVC     = {"net.connman", "/", "net.connman.Service"}
local CONNMAN_TECH    = {"net.connman", nil, "net.connman.Technology"}

local SVC           = {"com.service.TestConnMan",      "/com/service/TestConnMan",      nil}
local AGENT         = {"com.service.TestConnManAgent", "/com/service/TestConnManAgent", "net.connman.Agent"}
local AGENT_IFACE   = "net.connman.Agent"
local COUNTER_IFACE = "net.connman.Counter"
local NOTIFY_IFACE  = "net.connman.Notification"

-- Methods we register for the main agent API
local AGENT_METHODS = {

    { name = "Cancel",
      args = { }
    },
    { name = "Release",
      args = { }
    },
    { name = "RequestBrowser",
      args = {
             {name = "service", sig="o", dir="in"},
             {name = "url",     sig="s", dir="in"} }
    },
    { name = "ReportError",
      args = {
             {name = "service", sig="o", dir="in"},
             {name = "error",   sig="s", dir="in"} }
    },
    { name = "RequestInput",
      args = {
             {name = "service", sig="o",      dir="in"},
             {name = "fields",  sig="a{sv}",  dir="in"},
             {name = "input",   sig="a{sv}",  dir="out"} }
    },
}

-- Methods we register for the counter agent API
local COUNTER_AGENT_METHODS = {

    { name = "Release",
      args = { }
    },
    { name = "Usage",
      args = {
             {name = "service", sig="o",     dir="in"},
             {name = "home",    sig="a{sv}", dir="in"},
             {name = "roaming", sig="a{sv}", dir="in"} }
    },
}

-- Methods we register for the session notification API
local NOTIFY_METHODS = {

    -- Notification API
    { name = "Release",
      args = { }
    },
    { name = "Update",
      args = { {name = "settings", sig="a{sv}", dir="in"} }
    },
}

-- Globals
----------

local gDispatcher = l2dbus.Dispatcher.new(require("l2dbus_ev").MainLoop.new())
local gPrompter   = Prompter.getInstance(gDispatcher)
local gProxySvc
local gProxyCtrl
local gSystemConn

local gMgrProxy     -- table of {proxy,proxyCtrl}

local gSystemAgentConn
local gAgentSvc
local gAgentInf             -- Main agent interface
local gAgentMethods         = {}
local gCounterAgentInf      -- Counter agent interface
local gCounterAgentMethods  = {}
local gNotifyAPI            = {}

local gSignalRegistry       = {} -- prevents multiple registrations for same signal

-- option 'z' related variables
local gLastFunct       = nil
local gLastArgs        = nil
local gLastInfo        = ""

local gTechnologies    = {}
local gPrevOpt         = {}

local gArgs            = {}
local gVerbose         = 0

local gSessionList     = {}
local gLastSessionName = nil
local gLastSessionPath = nil


local GetSession

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
--- @tparam   (table)   tOptions  ....silent=bool for silent calls [default=true]
---                                   confirm=bool for confirmation prior to call [default=false]
---                                   noReply=bool for ignore DBus reply [default=false]
--- @tparam   (string)  functName ....
--- @tparam   (table)   proxy     ....{ proxy = proxy, proxyCtrl = proxyCtrl }
--- @tparam   (string)  ...       ....
---
--- @treturn  (boolean) [add here]
----------------------------------------------------------------
local function executeMenuAction(tOptions, proxyObj, functName, ... )

    tOptions = tOptions or {}

    -- Get a list of args for clarity to the user
    local sArgs = ""
    if arg then
        for k,v in pairs(arg) do
            if k ~= "n" and v then
                if type(v) == "table" then
                    sArgs = string.format("%s%s",sArgs, (sArgs=="") and pretty.write(v,"") or ","..pretty.write(v,""))
                else
                    sArgs = string.format("%s%s",sArgs, (sArgs=="") and tostring(v) or ","..tostring(v))
                end
            end
        end
    end

    assert( proxyObj.proxy,     "ERROR: Bad proxy sent to executeMenuAction" )
    assert( proxyObj.proxyCtrl, "ERROR: Bad proxyCtrl sent to executeMenuAction" )

    local func = proxyObj.proxy.m[functName]
    if func == nil then
        local sErr = "ERROR: proxy does not contain function: "..functName or ""
        print( sErr )
        return false, sErr
    end

    if tOptions.silent == false then

        -- Tracking for executing the last command easier
        gLastFunct = executeMenuAction
        gLastArgs  = { functName, func, unpack(arg) }
        gLastInfo  = string.format( "%s(%s)", functName, sArgs)
    end

    if tOptions.confirm == true then
        print(string.format( "\nCalling: %s(%s)", functName, sArgs))
        print( "Continue Request? (Y)es or (N)o [default is No]" )
        local cmd = gPrompter:getChar()
        print("\n")
        if cmd ~= "y" and cmd ~= "Y" then
            print( "Aborting..." )
            return
        end
    end

    if tOptions.silent == false then
        print("\nCalling:", functName)
    end

    if tOptions.noReply == true then
        proxyObj.proxyCtrl:setProxyNoReplyNeeded( true )
    end

    local bOk, status, result = pcall(func, unpack(arg))  -- EXECUTE METHOD

    --[[
    if tOptions.silent == false then
        if type(result) == "table" then
            print(functName, status, result, pretty.write(result))
        else
            print(functName, status, result)
        end
    end ]]

    if bOk == false then
        if tOptions.silent == false then
            print( "Error: ", status )
        end
        return status, result
    end

    if status then
        if not proxyObj.proxyCtrl:getBlockingMode() then
            if "userdata" == type(result) then
                local reply, errName, errMsg = proxyObj.proxyCtrl:waitForReply(result)
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

    if tOptions.silent == false then
        print( "Result:" )
    end

    if not status then
        if tOptions.silent == false then
            print("Error: " .. result)
            print()
        end
        return status, result

    elseif type(result) == "table" then
        -- Always a array because of getArgsAsArray
        local unpackedResult = unpack( result )
        if tOptions.silent == false then
            if type(unpackedResult) == "table" then
                pretty.dump( unpackedResult )
            else
                print( unpackedResult )
            end
        end
        print()
        return status, unpackedResult
    end

    -- Not likely unless a problem exists
    if tOptions.silent == false then
        print( result )
        print()
    end
    return status, result

end -- executeMenuAction

----------------------------------------------------------------
--- onAgentRequest
---
--- Simple generic handler for requests.
---
--- @tparam   (string)  prefix   .... "agent"|"counter"
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
---
--- @treturn  Always l2dbus.Dbus.HANDLER_RESULT_HANDLED
----------------------------------------------------------------
local function onAgentRequest(prefix, ifaceObj, conn, msg, userdata )

    local member  = msg:getMember()
    local methods = gAgentMethods

    if gVerbose > 0 then
        print("\nonAgentRequest: " .. ifaceObj:name() .. "/" .. member, prefix )
        dumpMessage(msg)
    end

    if prefix == "counter" then
        methods = gCounterAgentMethods
    elseif prefix == "notify" then
        methods = gNotifyAPI
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

    print("\nDefault Handler: " .. svcObj:path() .. "/" .. msg:getMember() )
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
    local function initAgent()

        -- Init ConnMan Agent Interface
        --------------------------------
        gSystemAgentConn = gSystemConn
        assert( nil ~= gSystemAgentConn )

        --requestName( gSystemAgentConn, AGENT[BUSNAME] )

        gAgentSvc = l2dbus.ServiceObject.new( AGENT[OBJPATH],
                                              defaultSvcHandler,
                                              "DefaultHandler" )
        -- Main Agent Interface
        -- - - - - - - - - - - - -
        gAgentInf = l2dbus.Interface.new( AGENT_IFACE,
                                          function (ifaceObj, conn, msg, userdata)
                                              return onAgentRequest( "agent", ifaceObj, conn, msg, userdata )
                                          end,
                                          "MainAgentHandler" )
        gAgentInf:registerMethods( AGENT_METHODS )
        assert( gAgentSvc:addInterface( gAgentInf ) )

        -- Counter Agent Interface
        -- - - - - - - - - - - - - - -
        gCounterAgentInf = l2dbus.Interface.new( COUNTER_IFACE,
                                          function (ifaceObj, conn, msg, userdata)
                                              return onAgentRequest( "counter", ifaceObj, conn, msg, userdata )
                                          end,
                                          "CounterAgentHandler" )
        gCounterAgentInf:registerMethods( COUNTER_AGENT_METHODS )
        assert( gAgentSvc:addInterface( gCounterAgentInf ) )

        -- Introspection interface
        -- - - - - - - - - - - - - - -
        assert( gAgentSvc:addInterface( l2dbus.Introspection.new() ) )

        assert( gSystemAgentConn:registerServiceObject( gAgentSvc ) )

        -- Register the Main Agent
        executeMenuAction( {silent=true}, gMgrProxy, "RegisterAgent", AGENT[OBJPATH] )

        if gArgs.counter == true then
            -- Register the Counter Agent
            executeMenuAction( {silent=true}, gMgrProxy, "RegisterCounter", AGENT[OBJPATH], gArgs.accuracy, gArgs.period )
        end

    end -- initAgent
    -- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    local function initMainProxy()

        -- Init Proxy to ConnMan
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

        -- Setup proxy to connman
        local proxyCtrl = proxyctrl.new(gSystemConn, CONNMAN_MGR[BUSNAME], CONNMAN_MGR[OBJPATH] )
        assert(proxyCtrl:bind())

        local proxy = proxyCtrl:getProxy( CONNMAN_MGR[IFACE] )
        assert( nil ~= proxy )

        gMgrProxy = { proxy = proxy, proxyCtrl = proxyCtrl }

    end -- initMainProxy
    -- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    local function initMainSignals()
        gMgrProxy.proxyCtrl:connectSignal( CONNMAN_MGR[IFACE],
                             "PropertyChanged",
                             function ( sProperty, val )
                                 if type(val) == "table" then
                                     print( "\nManager PropertyChanged: ", sProperty, pretty.write(val) )
                                 else
                                     print( "\nManager PropertyChanged: ", sProperty, val )
                                 end
                             end )
        gMgrProxy.proxyCtrl:connectSignal( CONNMAN_MGR[IFACE],
                             "ServicesChanged",
                             function ( tSvc, tObj )
                                 print( "\nManager ServiceChanged: ", pretty.write(tSvc), pretty.write(tObj) )
                             end )
        gMgrProxy.proxyCtrl:connectSignal( CONNMAN_MGR[IFACE],
                             "TechnologyRemoved",
                             function ( sObjPath )
                                 print( "\nManager TechnologyRemoved: ", sObjPath )
                             end )
        gMgrProxy.proxyCtrl:connectSignal( CONNMAN_MGR[IFACE],
                             "TechnologyAdded",
                             function ( sObjPath, tProp )
                                 print( "\nManager TechnologyAdded: ", sObjPath, pretty.write(tProp) )
                             end )
    end -- initMainSignals
    -- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    initMainProxy( )

    initAgent( )

    initMainSignals()

end -- initDbus


----------------------------------------------------------------
--- IsSignalRegistered
---
--- [add description here]
---
--- @tparam   (string)  sObjPath    ....
--- @tparam   (string)  sIface      ....
--- @tparam   (string)  sName       ....
---
--- @treturn  (boolean) false if not previously registered.
----------------------------------------------------------------
local function IsSignalRegistered( sObjPath, sIface, sName )

    local uuid = string.format( "%s__%s__%s", sObjPath, sIface, sName )

    if gSignalRegistry[uuid] == nil then
        gSignalRegistry[uuid] = true
        return false
    end

    return true

end -- IsSignalRegistered

----------------------------------------------------------------
--- GetProxy
---
--- Get a specific proxy type so we can exercise the API
---
--- @tparam   (string)  sName ....
--- @tparam   (string)  tDbus ....
---
--- @treturn  (object) specific proxy
----------------------------------------------------------------
local function GetProxy( sName, tDbus, sObjPathOverride, bSkipSignalReg )

    local proxyCtrl = proxyctrl.new(gSystemConn, tDbus[BUSNAME], sObjPathOverride or tDbus[OBJPATH] )
    assert(proxyCtrl:bind())

    if bSkipSignalReg ~= true and
       IsSignalRegistered( sObjPathOverride or tDbus[OBJPATH], tDbus[IFACE], sName.."_PropertyChanged" ) == false then

        proxyCtrl:connectSignal( tDbus[IFACE],
                             "PropertyChanged",
                             function ( sProperty, val )
                                 local sMsg = string.format( "\n%s PropertyChanged: ", sName )
                                 if type(val) == "table" then
                                     print( sMsg, sProperty, pretty.write(val) )
                                 else
                                     print( sMsg, sProperty, val )
                                 end
                             end )
    end

    local proxy = proxyCtrl:getProxy(tDbus[IFACE])
    assert( nil ~= proxy )

    return { proxy = proxy, proxyCtrl = proxyCtrl }

end -- GetProxy



----------------------------------------------------------------
--- submenuClock_get
---
--- [add description here]
----------------------------------------------------------------
local function submenuClock_get()

    local proxyObj = GetProxy( "Clock", CONNMAN_CLOCK )
    if proxyObj == nil then
        print( "ERROR: Unable to communicate with the clock API." )
        return
    end

    executeMenuAction( {silent=false}, proxyObj, "GetProperties" )

    proxyObj = nil

end -- submenuClock_get

----------------------------------------------------------------
--- submenuClock_set
---
--- [add description here]
----------------------------------------------------------------
local function submenuClock_set()

    local proxyObj = GetProxy( "Clock", CONNMAN_CLOCK )
    if proxyObj == nil then
        print( "ERROR: Unable to communicate with the clock API." )
        return
    end

    local tMenu = {"TimezoneUpdates",
                   "Time        (manual TimezoneUpdates only)",
                   "Timezone    (manual TimezoneUpdates only)",
                   "Timeservers (manual TimezoneUpdates only)" }

    local opt = gPrompter:selection( nil, "Select a clock property to change:", tMenu )
    if opt == nil then
        print( "Aborting..." )
        proxyObj = nil
        return
    end

    if opt == "Time" then
        local iTime = gPrompter:selection( 0, "New Clock Time:" )
        if iTime == nil then
            print( "Aborting..." )
            proxyObj = nil
            return
        end

        executeMenuAction( {confirm=true,silent=false}, proxyObj, "SetProperty", opt, l2dbus.DbusTypes.Variant.new(iTime, "vt") )

    elseif opt == "Timezone" then

        gPrevOpt.timezone = gPrevOpt.timezone or "America/New_York"
        local sTz = gPrompter:selection( gPrevOpt.timezone, "Enter a Timezone (matches /usr/share/zoneinfo):" )
        if sTz == nil then
            print( "Aborting..." )
            proxyObj = nil
            return
        end
        gPrevOpt.timezone = sTz

        executeMenuAction( {confirm=true,silent=false}, proxyObj, "SetProperty", opt, l2dbus.DbusTypes.Variant.new(sTz, "vs") )

    elseif opt == "TimezoneUpdates" then

        gPrevOpt.tzupdates = gPrevOpt.tzupdates or "auto"
        local sTzUpdates = gPrompter:selection( gPrevOpt.tzupdates, "Timezone Updates:", {"auto", "manual"} )
        if sTzUpdates == nil then
            print( "Aborting..." )
            proxyObj = nil
            return
        end
        gPrevOpt.tzupdates = sTzUpdates

        executeMenuAction( {confirm=true,silent=false}, proxyObj, "SetProperty", opt, l2dbus.DbusTypes.Variant.new(sTzUpdates, "vs") )

    elseif opt == "Timezoneservers" then

        print( "TODO" )
        print( )
    end

    proxyObj = nil

end -- submenuClock_set

----------------------------------------------------------------
--- submenu_Clock
---
--- Clock submenu
----------------------------------------------------------------
local function submenu_Clock(  )

    local tSubMenu = { "g. GetProperties",
                       "s. SetProperties",
                       "q. Exit to Main Menu" }
    while true do
        print( "" )
        local opt = gPrompter:selection( "g", "Select a Clock API option:", tSubMenu )
        if opt == "q" then
            return
        end

        if opt == "g" then
            submenuClock_get( )
        elseif opt == "s" then
            submenuClock_set( )
        end

    end -- while loop

end -- submenu_Clock


----------------------------------------------------------------
--- submenuManager_generic
---
--- GetProperties:
--- -------------------------------------
--- Returns all global system properties. See the
--- properties section for available properties.
---
--- Possible Errors: [service].Error.InvalidArguments
---
--- GetTechnologies:
--- -------------------------------------
--- Returns a list of tuples with technology object
--- path and dictionary of technology properties.
---
--- Possible Errors: [service].Error.InvalidArguments
---
--- GetServices:
--- -------------------------------------
--- Returns a sorted list of tuples with service
--- object path and dictionary of service properties.
---
--- This list will not contain sensitive information
--- like passphrases etc.
---
--- Possible Errors: [service].Error.InvalidArguments
---
----------------------------------------------------------------
local function submenuManager_generic( functName, ... )

    local status, tSvc = executeMenuAction( {silent=false}, gMgrProxy, functName, ... )

    if functName == "GetServices" and status == true then
        print( "\nGetServices Summary:" )
        print( "--------------------" )
        for k,v in pairs(tSvc) do
            if v and v[2] then
                local sIpInfo = ""
                if v[2].IPv4 and v[2].IPv4.Address then
                    sIpInfo = string.format( "IPv4:%s", v[2].IPv4.Address )
                end
                print( string.format("%-15s (autoConn:%-5s, state:%-7s type:%-10s %s)",
                                     v[2].Name or "",
                                     tostring(v[2].AutoConnect) or "",
                                     tostring(v[2].State) or "",
                                     v[2].Type or "",
                                     sIpInfo ) )
            end
        end
    end

end -- submenuManager_generic

----------------------------------------------------------------
--- submenuManager_createSession
---
--- Create a new session for the application. Every
--- application can create multiple session with
--- different settings. The settings are described
--- as part of the session interface.
---
--- The notifier allows asynchronous notification about
--- session specific changes. These changes can be
--- for online/offline state or IP address changes or
--- similar things the application is required to
--- handle.
---
--- Every application should at least create one session
--- to inform about its requirements and it purpose.
----------------------------------------------------------------
local function submenuManager_createSession( )

    -- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    local function Notification( notify_path )

        local proxySvc = l2dbus.ServiceObject.new( notify_path,
                                                   defaultSvcHandler,
                                                   "DefaultHandler" )
        -- Introspection interface
        assert( proxySvc:addInterface( l2dbus.Introspection.new() ) )

        local iface = l2dbus.Interface.new( NOTIFY_IFACE,
                                            function (ifaceObj, conn, msg, userdata)
                                                return onAgentRequest( "notify", ifaceObj, conn, msg, userdata )
                                            end,
                                            "NotifyHandler" )
        iface:registerMethods( NOTIFY_METHODS )
        assert( proxySvc:addInterface( iface ) )

        assert( gSystemConn:registerServiceObject( proxySvc ) )

        return { proxy = proxySvc, iface = iface }

    end -- Notification
    -- - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    local appPath     = gPrompter:selection( "/foo", "Enter a application path:" )
    local sessionName = gPrompter:selection( gLastSessionName or "sessionBar", "Enter a session name:" )

    local allowBearers = {}
    local opt          = ""

    -- AllowedBearers
    local tMenu = { "*","ethernet","wifi","bluetooth","cellular","vpn" }
    while opt ~= "*" do
        opt = gPrompter:selection( "*", "Select the bearer order (Enter to exit loop):", tMenu )
        if opt == nil then
            print( "Aborting..." )
            return
        end
        if #allowBearers == 0 or opt ~= "*" then
            table.insert( allowBearers, opt )
        end
        print( "Bearer Order:", pretty.write( allowBearers, " " ) )
    end

    -- Connection Type
    local tMenu    = { "any","internet","local" }
    local connType = gPrompter:selection( "any", "Select the connection type:", tMenu )

    -- Create the session
    local sessionObj = gSessionList[sessionName]

    if sessionObj and sessionObj.session then
        print( string.format( "WARNING: Session %s already created -> Creating another?", sessionName ) )
    else
        sessionObj = {}

        sessionObj.notify_path = SVC[OBJPATH] .. "/session/" .. tostring( sessionName )
        sessionObj.notify      = Notification( sessionObj.notify_path )
    end

    -- User configured settings
    sessionObj.settings = { AllowedBearers = allowBearers,
                            ConnectionType = connType }


    local cuid = posix.getpasswd( posix.getpid( "uid" ), "name") -- get the name
    local uid  = gPrompter:selection( cuid, "Create as uid (user):" )
    if uid == nil or uid == "" then
        uid = cuid
    end

    if posix.getpasswd( uid ) == nil then
        uid = cuid
        print( string.format("ERROR: %q is NOT a valid user--option ignored", uid) )
    end

    if uid ~= cuid then
        -- change to new uid, this ensures the session
        -- is created using the desired user ID
        local status, err = posix.setpid( "U", posix.getpasswd( uid, "uid" ) )
        if status ~= 0 then
            print( string.format("ERROR: Aborting, failed to change to uid: %s, err: %s", uid, err) )
            return
        else
            print( "Changed to user:", uid )
        end
    end

    local status, result = executeMenuAction( {confirm=true,silent=false},
                                      gMgrProxy, "CreateSession",
                                      sessionObj.settings,
                                      sessionObj.notify_path )
    if status then
        sessionObj.session_path = result
        print( "notify path: ", sessionObj.notify_path )
        print( "session path:", sessionObj.session_path )
        -- Setup the session interface to connman
        sessionObj.session = GetProxy( "", CONNMAN_SESSION, sessionObj.session_path, true )
        gSessionList[sessionName] = sessionObj

        gLastSessionName = sessionName
    end

    if uid ~= cuid then
        -- reset to original uid
        local status, err = posix.setpid( "U", posix.getpasswd( cuid, "uid" ) )
        if status ~= 0 then
            print( string.format("ERROR: Failed to change back to uid: %s, err: %s", cuid, err) )
        else
            print( "Changed back to user:", cuid )
        end
    end

end -- submenuManager_createSession

----------------------------------------------------------------
--- submenuManager_destroySession
---
--- Remove the previously created session.
---
--- If an application exits unexpectatly the session
--- will be automatically destroyed.
----------------------------------------------------------------
local function submenuManager_destroySession( )

    local sessionPath,_,proxyObj = GetSession( )

    local status = executeMenuAction( {confirm=true,silent=false},
                                      gMgrProxy, "DestroySession", sessionPath )
    proxyObj = nil

end -- submenuManager_destroySession

----------------------------------------------------------------
--- submenuManager_relPriv
---
--- Releases a private network.
---
--- Possible Errors: [service].Error.InvalidArguments
----------------------------------------------------------------
local function submenuManager_relPriv( )

    print( "ReleasePrivateSession TODO\n" )

end -- submenuManager_relPriv

----------------------------------------------------------------
--- submenuManager_reqPriv
---
--- Request a new Private Network, which includes the
--- creation of a tun/tap interface, and IP
--- configuration, NAT and IP forwarding on that
--- interface.
--- An object path, a dictionnary and a file descriptor
--- with IP settings are returned.
---
--- Possible Errors: [service].Error.InvalidArguments
--- 		 [service].Error.NotSupported
----------------------------------------------------------------
local function submenuManager_reqPriv(  )

    print( "RequestPrivateSession TODO\n" )

end -- submenuManager_reqPriv

----------------------------------------------------------------
--- submenuManager_set
---
--- [add description here]
----------------------------------------------------------------
local function submenuManager_set( )

    local tMenu = {"OfflineMode",
                   "SessionMode" }

    local opt = gPrompter:selection( nil, "Select a manager property to change:", tMenu )
    if opt == nil then
        print( "Aborting..." )
        return
    end

    local bOpt = gPrompter:selection( nil, opt.." Enabled:", { true, false } )
    if bOpt == nil then
        print( "Aborting..." )
        return
    end

    executeMenuAction( {confirm=true,silent=false}, gMgrProxy, "SetProperty", opt, l2dbus.DbusTypes.Variant.new(bOpt, "vb") )

end -- submenuManager_set

----------------------------------------------------------------
--- submenu_Manager
---
--- Manager submenu
----------------------------------------------------------------
local function submenu_Manager(  )

    local tSubMenu = { "c. CreateSession [experimental API]",
                       "d. DestroySession [experimental API]",
                       "g. GetProperties",
                       "l. ReleasePrivateSession [experimental API]",
                       "r. RequestPrivateSession [experimental API]",
                       "s. SetProperties",
                       "t. GetTechnologies",
                       "v. GetServices",
                       "q. Exit to Main Menu" }
    while true do
        print( "" )
        local opt = gPrompter:selection( "g", "Select a Manager API option:", tSubMenu )
        if opt == "q" then
            return
        end

        if opt == "c" then
            submenuManager_createSession( )
        elseif opt == "d" then
            submenuManager_destroySession( )
        elseif opt == "g" then
            submenuManager_generic( "GetProperties" )
        elseif opt == "s" then
            submenuManager_set( )
        elseif opt == "l" then
            submenuManager_relPriv( )
        elseif opt == "r" then
            submenuManager_reqPriv( )
        elseif opt == "t" then
            submenuManager_generic( "GetTechnologies" )
        elseif opt == "v" then
            submenuManager_generic( "GetServices" )
        end

    end -- while loop

end -- submenu_Manager


----------------------------------------------------------------
--- GetService
---
--- Get the users service selection.
---
--- @treturn  (string) interface name
--- @treturn  (table)  service data (from GetServices)
--- @treturn  (object) return the current selected tech proxy
----------------------------------------------------------------
local function GetService( sPrompt )

    local tMenu = {}
    local status, tSvc = executeMenuAction( {silent=true, confirm=false}, gMgrProxy, "GetServices" )
    if status == true then
        for k,v in pairs(tSvc) do
            if v and v[2] then
                table.insert( tMenu, string.format("%-15s (autoConn:%-5s, state:%-7s type:%s)",
                                               v[2].Name or "",
                                               tostring(v[2].AutoConnect) or "",
                                               tostring(v[2].State) or "",
                                               v[2].Type or "" ) )
            end
        end
    end

    sPrompt = sPrompt or "Select a service:"

    local svcIdx = gPrompter:selection( nil, sPrompt, tMenu, true )
    if svcIdx == nil or svcIdx > #tMenu then
        print( "Aborting..." )
        return nil
    end

    local tObj     = tSvc[svcIdx]
    local proxyObj = GetProxy( "Service", CONNMAN_SVC, tObj[1] )
    if proxyObj == nil then
        print( "ERROR: Unable to communicate with the service API." )
        return tObj[1], tObj[2]
    end

    return tObj[1], tObj[2], proxyObj

end -- GetService

----------------------------------------------------------------
--- submenuService_clear
---
--- Clears the value of the specified property.
---
--- Properties cannot be cleared for hidden WiFi service
--- entries or provisioned services.
---
--- Possible Errors: [service].Error.InvalidArguments
--- 		 [service].Error.InvalidProperty
----------------------------------------------------------------
local function submenuService_clear()

    local _, tObj, proxyObj = GetService()
    if proxyObj == nil then
        return
    end

    local _, tObj, proxyObj = GetService()
    if proxyObj == nil then
        return
    end

    local tMenu = {
      "AutoConnect",
      "IPv4.Configuration",
      "IPv6.Configuration",
      "Nameservers.Configuration",
      "Domains.Configuration",
      "Proxy.Configuration",
      "Timeservers.Configuration",
    }

    local opt = gPrompter:selection( nil, "Select a service property to clear:", tMenu )
    if opt == nil then
        print( "Aborting..." )
        proxyObj = nil
        return
    end

    executeMenuAction( {confirm=true,silent=false}, proxyObj, "ClearProperty", opt )

    proxyObj = nil

end -- submenuService_clear

----------------------------------------------------------------
--- submenuService_generic
---
--- Connect:
--- -------------------------------------
--- Connect this service. It will attempt to connect
--- WiFi or Bluetooth services.
---
--- For Ethernet devices this method can only be used
--- if it has previously been disconnected. Otherwise
--- the plugging of a cable will trigger connecting
--- automatically. If no cable is plugged in this method
--- will fail.
---
--- This method call will only return in case of an
--- error or when the service is fully connected. So
--- setting a longer D-Bus timeout might be a really
--- good idea.
---
--- Calling Connect() on a hidden WiFi service entry will
--- query the missing SSID via the Agent API causing a
--- WiFi service with the given SSID to be scanned,
--- created and connected.
---
--- Possible Errors: [service].Error.InvalidArguments
---
---
--- Disconnect:
--- -------------------------------------
--- Disconnect this service. If the service is not
--- connected an error message will be generated.
---
--- On Ethernet devices this will disconnect the IP
--- details from the service. It will not magically
--- unplug the cable. When no cable is plugged in this
--- method will fail.
---
--- This method can also be used to abort a previous
--- connection attempt via the Connect method.
---
--- Hidden WiFi service entries cannot be disconnected
--- as they always stay in idle state.
---
--- Possible Errors: [service].Error.InvalidArguments
---
---
--- Remove:
--- -------------------------------------
--- A successfully connected service with Favorite=true
--- can be removed this way. If it is connected, it will
--- be automatically disconnected first.
---
--- If the service requires a passphrase it will be
--- cleared and forgotten when removing.
---
--- This is similar to setting the Favorite property
--- to false, but that is currently not supported.
---
--- In the case a connection attempt failed and the
--- service is in the State=failure, this method can
--- also be used to reset the service.
---
--- Calling this method on Ethernet devices, hidden WiFi
--- services or provisioned services will cause an error
--- message. It is not possible to remove these kind of
--- services.
---
--- Possible Errors: [service].Error.InvalidArguments
---
----------------------------------------------------------------
local function submenuService_generic( sCmd )

    local _,_, proxyObj = GetService()
    if proxyObj == nil then
        return
    end

    executeMenuAction( {confirm=true,silent=false}, proxyObj, sCmd )

    proxyObj = nil

end -- submenuService_generic

----------------------------------------------------------------
--- submenuService_move
---
--- If a service has been used before/after, this allows a
--- reorder of the favorite services.
---
--- Possible Errors: [service].Error.InvalidArguments
----------------------------------------------------------------
local function submenuService_move( bBefore )

    local _,_, proxyObj = GetService()
    if proxyObj == nil then
        return
    end

    local sCmd    = "MoveAfter"
    local sPrompt = "Select the relative service: "
    if bBefore == true then
        sPrompt = sPrompt.."(other service will be BEFORE this one)"
        sCmd    = "MoveBefore"
    else
        sPrompt = sPrompt.."(other service will be AFTER this one)"
    end

    local sRelObjPath,_,_ = GetService(sPrompt)
    if sRelObjPath == nil then
        return
    end

    executeMenuAction( {confirm=true,silent=false}, proxyObj, sCmd )

    proxyObj = nil

end -- submenuService_move

----------------------------------------------------------------
--- submenuService_set
---
--- Changes the value of the specified property. Only
--- properties that are listed as read-write are
--- changeable. On success a PropertyChanged signal
--- will be emitted.
---
--- Properties cannot be set for hidden WiFi service
--- entries or provisioned services.
---
--- Possible Errors: [service].Error.InvalidArguments
---          [service].Error.InvalidProperty
----------------------------------------------------------------
local function submenuService_set()

    local _, tObj, proxyObj = GetService()
    if proxyObj == nil then
        return
    end

    local tMenu = {
      "AutoConnect",
      "IPv4.Configuration",
      "IPv6.Configuration",
      "Nameservers.Configuration",
      "Domains.Configuration",
      "Proxy.Configuration",
      "Timeservers.Configuration",
    }

    local opt = gPrompter:selection( nil, "Select a service property to change:", tMenu )
    if opt == nil then
        print( "Aborting..." )
        proxyObj = nil
        return
    end

    if opt == "AutoConnect" then
        local bOpt = gPrompter:selection( not tObj.AutoConnect, opt.." Enabled:", { true, false } )
        if bOpt == nil then
            print( "Aborting..." )
            return
        end

        executeMenuAction( {confirm=true,silent=false}, proxyObj, "SetProperty", opt, l2dbus.DbusTypes.Variant.new(bOpt, "vb") )

    elseif opt == "IPv4.Configuration" then

        if tObj[opt] then
            print( string.format("Current %s Settings:", opt ))
            pretty.dump( tObj[opt] )
            print( )
        else
            print( "Note: nothing currently set" )
        end

        if tObj[opt].Method == "fixed" then
            print( 'Set to "fixed", unable to change\n' )
            return
        end

        local sMethod = gPrompter:selection( nil, "Method:", { "dhcp", "manual", "off" } )
        if sMethod == nil then
            print( "Aborting..." )
            return
        end

        if sMethod == "manual" then

            if gPrevOpt.ipv4 == nil then
                gPrevOpt.ipv4 = {}
            end

            gPrevOpt.ipv4.Address = gPrevOpt.ipv4.Address or "192.168.1.176"
            local sAddr = gPrompter:selection( gPrevOpt.ipv4.Address, "IPv4 Address: (e.g. 192.168.1.14)" )
            if sAddr == nil then
                print( "Aborting..." )
                return
            end
            gPrevOpt.ipv4.Address = sAddr

            local a,b,c,d  = string.match( gPrevOpt.ipv4.Address, "(%d+)%.(%d+)%.(%d+)%.(%d+)" )
            if not a or not b or not c or not d then
                print( "WARNING: You entered and invalid IPv4 address (e.g. 192.168.1.123)" )
            end

            local sMask = gPrompter:selection( gPrevOpt.ipv4.Netmask or "255.255.255.0", "IPv4 Netmask:" )
            if sMask == nil then
                print( "Aborting..." )
                return
            end
            gPrevOpt.ipv4.Netmask = sMask

            local sGateway = gPrevOpt.ipv4.Gateway
            if a and b and c and d then
                sGateway = string.format( "%s.%s.%s.1", a,b,c )
            end

            local sGateway = gPrompter:selection( sGateway, "IPv4 Gateway:" )
            if sGateway == nil then
                print( "Aborting..." )
                return
            end
            gPrevOpt.ipv4.Gateway = sGateway

            gPrevOpt.ipv4.Method = "manual" -- MUST be set

            executeMenuAction( {confirm=true,silent=false}, proxyObj, "SetProperty", opt, l2dbus.DbusTypes.Variant.new(gPrevOpt.ipv4) )

        else
            executeMenuAction( {confirm=true,silent=false}, proxyObj, "SetProperty", opt, l2dbus.DbusTypes.Variant.new({ Method = sMethod }) )
        end

    else
        print( "TODO" )
        print( )
    end

    proxyObj = nil

end -- submenuService_set

----------------------------------------------------------------
--- submenu_Service
---
--- Service submenu
----------------------------------------------------------------
local function submenu_Service(  )

    local tSubMenu = { "a. MoveAfter",
                       "b. MoveBefore",
                       "c. Connect",
                       "d. Disconnect",
                       "g. GetProperties (uses Manager.GetServices)",
                       "l. ClearProperty",
                       "r. Remove",
                       "s. SetProperties",
                       "t. ResetCounters [Experimental]",
                       "q. Exit to Main Menu" }
    while true do
        print( "" )
        local opt = gPrompter:selection( "", "Select a Service API option:", tSubMenu )
        if opt == "q" then
            return
        end

        if opt == "a" then
            submenuService_move( )
        elseif opt == "b" then
            submenuService_move( true )
        elseif opt == "c" then
            submenuService_generic( "Connect" )
        elseif opt == "d" then
            submenuService_generic( "Disconnect" )
        elseif opt == "g" then
            submenuManager_generic( "GetServices" )
        elseif opt == "l" then
            submenuService_clear( )
        elseif opt == "r" then
            submenuService_generic( "Remove" )
        elseif opt == "s" then
            submenuService_set( )
        elseif opt == "t" then
            submenuService_generic( "ResetCounters" )
        end

    end -- while loop

end -- submenu_Service


----------------------------------------------------------------
--- GetSession
---
--- Get the users session selection.
---
--- @treturn  (string) sesison name
--- @treturn  (table)  service data (from GetSession)
--- @treturn  (object) return the current selected session proxy
----------------------------------------------------------------
function GetSession( bSkipManualEntry )

    local tMenu = {}

    -- Until the manager supports a GetSession like interface
    -- we will only be able to display sessions we know about.
    local tSvc = {}
    for k,v in pairs(gSessionList) do
        table.insert( tMenu, string.format("%-10s (objPath:%-25s)",
                                           k, v.session_path ))
        table.insert( tSvc, { k, v } )
    end
    if bSkipManualEntry ~= true then
        table.insert( tMenu, "[manually enter]" )
    elseif #tMenu == 0 then
        print( "No Sessions..." )
        return nil
    end

--  local status, tSvc = executeMenuAction( {silent=true, confirm=false}, gMgrProxy, "GetSession" )
--  if status == true then
--      for k,v in pairs(tSvc) do
--          TBD
--      end
--  end

    local svcIdx = gPrompter:selection( nil, "Select a session:", tMenu, true )
    if svcIdx == nil or svcIdx > #tMenu then
        print( "Aborting..." )
        return nil
    end

    local tObj = tSvc[svcIdx]

    local sessionPath = ""
    if tObj == nil then
        if bSkipManualEntry ~= true then
            sessionPath = gPrompter:selection( gLastSessionPath, "Enter a session object path:" )
        else
            print( "Aborting..." )
            return nil
        end
    else
        sessionPath = tObj[2].session_path
    end

    local proxyObj = GetProxy( nil, CONNMAN_SESSION, sessionPath, true )
    if proxyObj == nil then
        print( "ERROR: Unable to communicate with the session API." )
        return tObj[1], tObj[2]
    end

    gLastSessionPath = sessionPath

    return tObj[1], tObj[2], proxyObj

end -- GetSession

----------------------------------------------------------------
--- submenuSession_change
---
--- Change the value of certain settings. Not all
--- settings can be changed. Normally this should not
--- be needed or an extra session should be created.
--- However in some cases it makes sense to change
--- a value and trigger different behavior.
---
--- A change of a setting will cause an update notification
--- to be sent. Some changes might cause the session to
--- be moved to offline state.
----------------------------------------------------------------
local function submenuSession_change( )

    print( "Change TODO\n" )

end -- submenuSession_change

----------------------------------------------------------------
--- submenuSession_connect
---
--- If not connected, then attempt to connect this
--- session.
---
--- The usage of this method depends a little bit on
--- the model of the application. Some application
--- should not try to call Connect on any session at
--- all. They should just monitor if it becomes online
--- or gets back offline.
---
--- Others might require an active connection right now.
--- So for example email notification should only check
--- for new emails when a connection is available. However
--- if the user presses the button for get email or wants
--- to send an email it should request to get online with
--- this method.
---
--- Depending on the bearer settings the current service
--- is used or a new service will be connected.
---
--- This method returns immediately after it has been
--- called. The application is informed through the update
--- notification about the state of the session.
---
--- It is also not guaranteed that a session stays online
--- after this method call. It can be taken offline at any
--- time. This might happen because of idle timeouts or
--- other reasons.
---
--- It is safe to call this method multiple times. The
--- actual usage will be sorted out for the application.
----------------------------------------------------------------
local function submenuSession_connect( )

    local _,_,proxyObj = GetSession( )

    if proxyObj then
        executeMenuAction( {confirm=true,silent=false}, proxyObj, "Connect" )
    end

    proxyObj = nil

end -- submenuSession_connect

----------------------------------------------------------------
--- submenuSession_disconnect
---
--- This method indicates that the current session does
--- not need a connection anymore.
---
--- This method returns immediately. The application is
--- informed through the update notification about the
--- state of the session.
----------------------------------------------------------------
local function submenuSession_disconnect( )

    local _,_,proxyObj = GetSession( )

    if proxyObj then
        executeMenuAction( {confirm=true,silent=false}, proxyObj, "Disconnect" )
    end

    proxyObj = nil

end -- submenuSession_disconnect

----------------------------------------------------------------
--- submenuSession_get
---
--- This method will list all sessions and get the details for
--- one session.
---
--- NOTE: Currently not part of the connman API; therefore, we
---       can only list the sessions created by this tool.
----------------------------------------------------------------
local function submenuSession_get( )

    local sessionPath,tObj,proxyObj = GetSession( true )

    if tObj then
        print( "Session:", sessionPath )
        pretty.dump( tObj )
    end

    proxyObj = nil

end -- submenuSession_get

----------------------------------------------------------------
--- submenu_Session
---
--- Session submenu
----------------------------------------------------------------
local function submenu_Session(  )

    local tSubMenu = { "c. CreateSession [experimental Manager API]",
                       "d. DestroySession [experimental Manager API]",
                       "g. GetSession (not part of connman)",
                       "h. Change",
                       "m. Connect",
                       "n. Disconnect",
                       "q. Exit to Main Menu" }
    while true do

        print( "" )
        local opt = gPrompter:selection( "", "Select a Session API option:", tSubMenu )
        if opt == "q" then
            return
        end

        if opt == "c" then
            submenuManager_createSession( )
        elseif opt == "d" then
            submenuManager_destroySession( )
        elseif opt == "g" then
            submenuSession_get()
        elseif opt == "h" then
            submenuSession_change()
        elseif opt == "m" then
            submenuSession_connect( )
        elseif opt == "n" then
            submenuSession_disconnect( )
        end

    end -- while loop

end -- submenu_Session

----------------------------------------------------------------
--- GetTech
---
--- Get the list of technologies available and get a selection
--- from the user.
---
--- @tparam   (string)  bShowAllOpt ....add an "all" option to the list
--- @tparam   (string)  bRetType    ....return the Type instead of the name
---
--- @treturn  (string) return the name or type of the technology
--- @treturn  (table)  return the current selected tech settings
--- @treturn  (object) return the current selected tech proxy
----------------------------------------------------------------
local function GetTech( bShowAllOpt, bRetType )

    local tMenu = {}
    local status, tTech = executeMenuAction( {silent=true, confirm=false}, gMgrProxy, "GetTechnologies" )
    if status == true then
        for k,v in pairs(tTech) do
            if v and v[2] then
                table.insert( tMenu, string.format("%-15s (conn:%5s, pwr:%5s, teth:%5s, type:%s)",
                                               v[2].Name or "",
                                               tostring(v[2].Connected) or "",
                                               tostring(v[2].Powered) or "",
                                               tostring(v[2].Tethering) or "",
                                               v[2].Type or "" ) )
            end
        end
    end

    local sDefault = nil
    if bShowAllOpt == true then
        table.insert( tMenu, "all" )
        sDefault = "all"
    end

    local opt = gPrompter:selection( sDefault, "Select a technology:", tMenu )

    if opt == "all" then
        return opt, tTech
    end

    -- Lookup the selected object
    local tObj  = nil
    local sType = string.match( opt, ".*type:(.*)%).*" )
    local sName = string.match( opt, "(%S+)%s.*" )
    for k,v in pairs(tTech) do
        if bRetType == true then
            if v and v[2] and v[2].Type == sType then
                tObj = v
            end
        else
            if v and v[2] and v[2].Name == sName then
                tObj = v
            end
        end
    end

    print( tObj[1] )

    local proxyObj = GetProxy( "Tech", CONNMAN_TECH, tObj[1] )
    if proxyObj == nil then
        print( "ERROR: Unable to communicate with the technology API." )
        return
    end

    if bRetType == true then
        return sType, tObj, proxyObj
    end

    return sName, tObj, proxyObj

end -- GetTech

----------------------------------------------------------------
--- submenuTech_get
---
--- [add description here]
----------------------------------------------------------------
local function submenuTech_get()

    local _, tTech, _ = GetTech( true )
    pretty.dump( tTech )

end -- submenuTech_get

----------------------------------------------------------------
--- submenuTech_scan
---
--- Trigger a scan for this specific technology. The
--- method call will return when a scan has been
--- finished and results are available. So setting
--- a longer D-Bus timeout might be a really good
--- idea.
---
--- Results will be signaled via the ServicesChanged
--- signal from the manager interface.
---
----------------------------------------------------------------
---
--- The result is the errno, you can also look for connmand
--- output of:
--- connmand[1553]: src/device.c:__connman_device_request_scan() device 0xa7570 err -67
--- connmand[1553]: src/technology.c:reply_scan_pending() technology 0xb4470 err -67
---
---   0  == SUCCESS
--- -67  == -ENOLINK     == device NOT powered
--- -95  == -EPNOTSUPP   == no driver OR no driver scan support
--- -114 == -EALREADY    == already scanning
--- -115 == -EINPROGRESS == already scanning
----------------------------------------------------------------
local function submenuTech_scan()

    local _, tTech, proxyObj = GetTech( )

    executeMenuAction( {confirm=true,silent=false,noReply=true}, proxyObj, "Scan" )

    proxyObj = nil

end -- submenuTech_scan

----------------------------------------------------------------
--- submenuTech_set
---
--- [add description here]
----------------------------------------------------------------
local function submenuTech_set()

    local sName, tTech, proxyObj = GetTech()

    print( string.format("Current %s Settings:", sName or "unknown"))
    pretty.dump( tTech )
    print()

    if tTech == nil then
        tTech = {"",{}}
    end

    local tMenu = {"Powered",
                   "Tethering" }

    if tTech[2].Type == "wifi" then
        table.insert(tMenu, "TetheringIdentifier (SSID)")
        table.insert(tMenu, "TetheringPassphrase (WPA pre-shared key)")
    end

    local opt = gPrompter:selection( nil, "Select a technology property to change:", tMenu )
    if opt == nil then
        print( "Aborting..." )
        proxyObj = nil
        return
    end

    if opt == "Powered" or opt == "Tethering" then
        local bOpt = gPrompter:selection( not tTech[2][opt], opt.." Enabled:", { true, false } )
        if bOpt == nil then
            print( "Aborting..." )
            proxyObj = nil
            return
        end

        executeMenuAction( {confirm=true,silent=false}, proxyObj, "SetProperty", opt, l2dbus.DbusTypes.Variant.new(bOpt, "vb") )

    elseif opt == "TetheringIdentifier (SSID)" then

        gPrevOpt.SSID = gPrevOpt.SSID or "ssid"
        opt = gPrompter:selection( gPrevOpt.SSID, "Enter an SSID:" )
        if opt == nil then
            print( "Aborting..." )
            proxyObj = nil
            return
        end
        gPrevOpt.SSID = opt

        executeMenuAction( {confirm=true,silent=false}, proxyObj, "SetProperty", "TetheringIdentifier", opt )

    elseif opt == "TetheringPassphrase (WPA pre-shared key)" then

        gPrevOpt.WPAkey = gPrevOpt.WPAkey or "password"
        opt = gPrompter:selection( gPrevOpt.WPAkey, "Enter an WPA pre-shared key:" )
        if opt == nil then
            print( "Aborting..." )
            proxyObj = nil
            return
        end
        gPrevOpt.WPAkey = opt

        executeMenuAction( {confirm=true,silent=false}, proxyObj, "SetProperty", "TetheringPassphrase", opt )

    end

    proxyObj = nil

end -- submenuTech_set

----------------------------------------------------------------
--- submenu_Tech
---
--- Technologies submenu
----------------------------------------------------------------
local function submenu_Tech(  )

    local tSubMenu = { "c. Scan",
                       "g. GetProperties",
                       "s. SetProperties",
                       "q. Exit to Main Menu" }
    while true do
        print( "" )
        local opt = gPrompter:selection( "", "Select a Technology API option:", tSubMenu )
        if opt == "q" then
            return
        end

        if opt == "c" then
            submenuTech_scan( )
        elseif opt == "g" then
            submenuTech_get( )
        elseif opt == "s" then
            submenuTech_set( )
        end

    end -- while loop

end -- submenu_Tech



----------------------------------------------------------------
--- menuMain
---
--- Main menu of options
----------------------------------------------------------------
local function menuMain()

    local doExit = false
    while not doExit do

        -- Read the list of technologies
        -- Sample results:
        --[[ {
              {
                "/net/connman/technology/ethernet",
                {
                  Type = "ethernet",
                  Name = "Wired",
                  Tethering = false,
                  Powered = true,
                  Connected = true
                }
              },
              {
                "/net/connman/technology/bluetooth",
                {
                  Type = "bluetooth",
                  Name = "Bluetooth",
                  Tethering = false,
                  Powered = false,
                  Connected = false
                }
              },
              {
                "/net/connman/technology/wifi",
                {
                  Type = "wifi",
                  Name = "WiFi",
                  Tethering = false,
                  Powered = false,
                  Connected = false
                }
              }
            } ]]
        local sTech = ""
        local status, tTech = executeMenuAction( {silent=true, confirm=false}, gMgrProxy, "GetTechnologies" )
        if status == true then
            for k,v in pairs(tTech) do
                sTech = string.format("%s%s", (sTech == "") and "" or sTech..", ", v[2].Name or "" )
            end
        else
            sTech = "unknown"
        end

        print("\nLua D-Bus Connman Test Client v"..APP_VER)
        print(string.rep("=", 50))
        print()

        print("c.   Clock Submenu [Experimental API]")
        print("m.   Manager Submenu")
        print("s.   Session Submenu")
        print(string.format("t.   Technologies Submenu (%s)", sTech))
        print("v.   Service Submenu")

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
        elseif cmd == 'c' then
            submenu_Clock()
        elseif cmd == 'm' then
            submenu_Manager()
        elseif cmd == 's' then
            submenu_Session()
        elseif cmd == 't' then
            submenu_Tech()
        elseif cmd == 'v' then
            submenu_Service()
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
---                             ...."prompt", line of text for input
---                                  nil, just display prompt, no input needed
---
--- @treturn  (string)  "YorN", returns "Y" or "N"
----------------------------------------------------------------
local function popup( sPrompt, sTitle, sType )

    sTitle = sTitle or "Prompt"
    sTitle = sTitle.." |"

    print( "" )
    print( string.rep( "*", 60 ) )
    print( string.format( "* %-56s *", sTitle ) )
    print( string.format( "* %-56s *", string.rep("-", #sTitle) ) )
    print( string.format( "* %-56s *", "" ) )
    print( string.format( "* %-56s *", "" ) )
    local sLines = utils.split( sPrompt, "\n" )
    for _, sLine in pairs(sLines) do
        print( string.format( "* %-56s *", sLine ) )
    end
    print( string.format( "* %-56s *", os.date() ) )
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
--- agent_Cancel
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
function gAgentMethods.agent_Cancel( ifaceObj, conn, msg, userdata )

    popup( "Agent Cancelled", "Main Agent API" )
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- agent_Cancel

----------------------------------------------------------------
--- agent_Release
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
function gAgentMethods.agent_Release( ifaceObj, conn, msg, userdata )

    popup( "Release", "Main Agent API" )
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- agent_Release

----------------------------------------------------------------
--- agent_RequestBrowser
---
--- void RequestBrowser()
---
--- This method gets called when it is required
--- to ask the user to open a website to procceed
--- with login handling.
---
--- This can happen if connected to a hotspot portal
--- page without WISPr support.
---
--- Possible Errors: net.connman.Agent.Error.Canceled
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gAgentMethods.agent_RequestBrowser( ifaceObj, conn, msg, userdata )

    local sMsg = string.format("Request Browser\n%s\nResponsing with \"Canceled\"\n",
                               pretty.write( msg:getArgsAsArray() ))
    popup( sMsg, "Main Agent API" )
    conn:send( l2dbus.Message.newError( msg, "net.connman.Agent.Error.Canceled", "" ) )

end -- agent_RequestBrowser

----------------------------------------------------------------
--- agent_ReportError
---
--- void ReportError()
---
--- This method gets called when an error has to be
--- reported to the user.
---
--- A special return value can be used to trigger a
--- retry of the failed transaction.
---
--- Possible Errors: net.connman.Agent.Error.Retry
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gAgentMethods.agent_ReportError( ifaceObj, conn, msg, userdata )

    local sMsg = string.format("Release Error\n%s\n", pretty.write( msg:getArgsAsArray() ))
    popup( sMsg, "Main Agent API" )
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- agent_ReportError

----------------------------------------------------------------
--- agent_RequestInput
---
--- void RequestInput()
---
--- This method gets called when an error has to be
--- reported to the user.
---
--- A special return value can be used to trigger a
--- retry of the failed transaction.
---
--- Possible Errors: net.connman.Agent.Error.Retry
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....Example,
---                                 {
---                                   "/net/connman/service/wifi_00707e8cef29_54455354_managed_psk",
---                                   {
---                                     Passphrase = {
---                                       Alternates = {
---                                         "WPS"
---                                       },
---                                       Type = "psk",
---                                       Requirement = "mandatory"
---                                     },
---                                     WPS = {
---                                       Requirement = "alternate",
---                                       Type = "wpspin"
---                                     }
---                                   }
---                                 }
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gAgentMethods.agent_RequestInput( ifaceObj, conn, msg, userdata )

    local tRet  = {}
    local tArgs = msg:getArgsAsArray()
    local sTmp  = ""

    popup( string.format("Request Input\n%s\n", pretty.write( tArgs )), "Main Agent API" )

    gPrevOpt.input = gPrevOpt.input or {}

    local confirm = "n"
    while confirm == "n" or confirm == "N" do
        tRet  = {}
        -- Simple string args
        local tFields = {"Passphrase", "Name", "Identity",
                         "PreviousPassphrase",
                         "Username", "Password"} --"WPS" is ignored for now.
        for k,v in pairs(tFields) do
            if tArgs[2][v] then
                local sPrompt = string.format( "%s (%s.%s):", v, tArgs[2][v].Type or "unknown", tArgs[2][v].Requirement or "" )
                if tArgs[2][v].Value then
                    sPrompt = string.format("%s (%s)", sPrompt,tArgs[2][v].Value)
                end

                if tArgs[2][v].Requirement == "informational" then
                    print( sPrompt )
                else
                    print( sPrompt )
                    sTmp = io.stdin:read( "*line" ) -- block
                    print( "\n" )
                    gPrevOpt.input[v] = sTmp
                    tRet[v]           = sTmp
                end
            end
        end

        print(string.format( "\nReply with: %s", pretty.write(tRet,"") ))
        print( "Continue? (Y)es or (N)o [default is No] or (I)gnore" )
        confirm = io.stdin:read( "*line" ) -- block
        print("\n")
        if confirm == "i" and confirm == "I" then
            conn:send( l2dbus.Message.newMethodReturn( msg ) )
            return
        end
    end

    local replyMsg = l2dbus.Message.newMethodReturn( msg )
    replyMsg:addArgs( tRet )

    conn:send( replyMsg )

end -- agent_RequestInput


----------------------------------------------------------------
--- counter_Release
---
--- void Release()
---
--- This method gets called when the service daemon
--- unregisters the counter. A counter can use it to do
--- cleanup tasks. There is no need to unregister the
--- counter, because when this method gets called it has
--- already been unregistered.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gCounterAgentMethods.counter_Release( ifaceObj, conn, msg, userdata )

    popup( "Release", "Counter Agent API" )
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- counter_Release

----------------------------------------------------------------
--- counter_Usage
---
--- void Usage()
---
--- This method gets called when the service daemon
--- unregisters the counter. A counter can use it to do
--- cleanup tasks. There is no need to unregister the
--- counter, because when this method gets called it has
--- already been unregistered.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gCounterAgentMethods.counter_Usage( ifaceObj, conn, msg, userdata )

    local sMsg = string.format("Usage\n%s\n",
                               pretty.write( msg:getArgsAsArray() ))
    popup( sMsg, "Counter Agent API" )
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- counter_Usage

----------------------------------------------------------------
--- notify_Release
---
--- Callback for the notification Release API
---
--- This method gets called when the service daemon
--- unregisters the session. A counter can use it to do
--- cleanup tasks. There is no need to unregister the
--- session, because when this method gets called it has
--- already been unregistered.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gNotifyAPI.notify_Release( ifaceObj, conn, msg, userdata )

    local session_name = tostring(msg:getObjectPath())
    local sessionObj   = gSessionList[session_name]

    local sMsg = string.format("Release\nsession: %s\n%s",
                               session_name,
                               pretty.write( sessionObj ))
    popup( sMsg, "Notify Agent API" )

    if sessionObj then
        -- Release
        sessionObj.session         = nil
        sessionObj.notify          = nil
        gSessionList[session_name] = nil
    end

    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- notify_Release

----------------------------------------------------------------
--- notify_Update
---
--- Callback for the notification Update API
---
--- Sends an update of changed settings. Only settings
--- that are changed will be included.
---
--- Initially on every session creation this method is
--- called once to inform about the current settings.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gNotifyAPI.notify_Update( ifaceObj, conn, msg, userdata )

    local sMsg = string.format("Update\nsession: %s\n%s",
                               tostring(msg:getObjectPath()),
                               pretty.write( msg:getArgsAsArray() ))
    popup( sMsg, "Notify Agent API" )
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- notify_Update

----------------------------------------------------------------
--- parseArgs
---
--- Parse command line arguments
----------------------------------------------------------------
local function parseArgs()

    gArgs = lapp [[
 Command Options:
   -a,--accuracy    (default 50)     Optional: Counter accuracy in kB
   -c,--counter                      Optional: Register counter agent,
                                               default false (counter agent NOT registered)
   -p,--period      (default 60)     Optional: Counter reporting period
   -v...                             Verbosity

 ]]

    gVerbose = not gArgs.v[1] and 0 or #gArgs.v

end -- parseArgs

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

    gMgrProxy.proxyCtrl:disconnectAllSignals()
    gPrompter:restoreTtyState()

    gMgrProxy       = nil
    gPrompter       = nil
    gDispatcher     = nil

end -- main


parseArgs( )

main( )

l2dbus.shutdown()


--[=[

Example: Manager.GetServices
--------------------------------------------------------
{
  {
    "/net/connman/service/ethernet_0019b800faea_cable",
    {
      ["IPv6.Configuration"] = {
        Method = "off"
      },
      IPv4 = {
        Netmask = "255.255.255.0",
        Gateway = "192.168.1.1",
        Method = "dhcp",
        Address = "192.168.1.14"
      },
      IPv6 = {
      },
      Nameservers = {
        "192.168.1.1"
      },
      Immutable = false,
      AutoConnect = true,
      ["Domains.Configuration"] = {
      },
      Type = "ethernet",
      Provider = {
      },
      ["Proxy.Configuration"] = {
      },
      Proxy = {
        Method = "direct"
      },
      Domains = {
      },
      Timeservers = {
        "192.168.1.1"
      },
      State = "online",
      ["Timeservers.Configuration"] = {
      },
      Name = "Wired",
      Ethernet = {
        Address = "00:19:B8:00:FA:EA",
        MTU = 1500,
        Method = "auto",
        Interface = "eth0"
      },
      ["Nameservers.Configuration"] = {
      },
      ["IPv4.Configuration"] = {
        Method = "dhcp"
      },
      Favorite = true,
      Security = {
      }
    }
  },
  {
    "/net/connman/service/bluetooth_001834436983_d8d1cba8aa13",
    {
      ["IPv6.Configuration"] = {
        Method = "off"
      },
      IPv4 = {
      },
      IPv6 = {
      },
      Nameservers = {
      },
      Immutable = false,
      AutoConnect = false,
      ["Domains.Configuration"] = {
      },
      Type = "bluetooth",
      Provider = {
      },
      ["Proxy.Configuration"] = {
      },
      Proxy = {
      },
      Domains = {
      },
      Timeservers = {
      },
      State = "idle",
      ["Timeservers.Configuration"] = {
      },
      Name = "tPhone",
      Ethernet = {
        Method = "auto"
      },
      ["Nameservers.Configuration"] = {
      },
      ["IPv4.Configuration"] = {
        Method = "dhcp"
      },
      Favorite = false,
      Security = {
      }
    }
  }
}

]=]


