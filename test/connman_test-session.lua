#!/usr/bin/env lua
-------------------------------------------------------------------------------
--- connman "test-session" in Lua
---
--- This is a functional replacement to the python test-session tool.  This
--- is meant as both an example and a replacement when Lua is available and
--- python is not.  This example is also not meant to extend the tool other
--- than some improved argument checking, etc.  Altered client testing will
--- be done in a different example.
---
--- NOTE: This example uses the lower level serviceObject interface, not the
---       service interface that is recommended.
-------------------------------------------------------------------------------

local proxyctrl = require("l2dbus.proxyctrl")
local l2dbus    = require("l2dbus")
local pretty    = require("pl.pretty")
local tablex    = require("pl.tablex")
local utils     = require("pl.utils")

local gSessionConn
local gSystemConn
local gMgrProxy
local gSvcProxy      -- only when running as client

local gProxyIface
local gProxySvc

local gDispatcher   = l2dbus.Dispatcher.new(require("l2dbus_ev").MainLoop.new())

local gSessionAppObj

local BUSNAME         = 1
local OBJPATH         = 2
local IFACE           = 3
local CONNMAN_MGR     = {"net.connman", "/", "net.connman.Manager"}
local CONNMAN_SESSION = {"net.connman", nil, "net.connman.Session"}
local SVC             = {"com.example.SessionApplication.%s", "/com/example/SessionApplication",  "com.example.TestSession"}
local NOTIFY_IFACE    = "net.connman.Notification"

local gSessionAPI   = {}
local gNotifyAPI    = {}

local gSessionList  = {}

local NOTIFY_METHODS = {

    -- Notification API
    { name = "Release",
      args = { }
    },
    { name = "Update",
      args = { {name = "settings", sig="a{sv}", dir="in"} }
    },
}

local SVC_METHODS = {

    -- Session API
    { name = "Change",
      args = { {name = "session_name", sig="s",  dir="in"},
               {name = "key",          sig="s",  dir="in"},
               {name = "value",        sig="sv", dir="in"} }
    },
    { name = "Connect",
      args = { {name = "session_name", sig="s", dir="in"} }
    },
    -- Destroy
    { name = "Disconnect",
      args = { {name = "session_name", sig="s", dir="in"} }
    },

    -- Service API
    { name = "Configure",
      args = { {name = "session_name", sig="s",  dir="in"},
               {name = "key",          sig="s",  dir="in"},
               {name = "value",        sig="sv", dir="in"} }
    },
    { name = "CreateSession",
      args = { {name = "session_name", sig="s", dir="in"} }
    },
    { name = "DestroySession",
      args = { {name = "session_name", sig="s", dir="in"} }
    },
    { name = "Debug",
      args = { }
    },
}



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
--- defaultSvcHandler
---
--- Default service handler.  If this is seen there is likely
--- an exception in the original handlers code.
---
--- @tparam   (string)  svcObj   ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
---
--- @treturn  (number) l2dbus.Dbus.HANDLER_RESULT_HANDLED
----------------------------------------------------------------
local function defaultSvcHandler(svcObj, conn, msg, userdata)

    print("\nDefault API: " .. svcObj:path() .. "/" .. msg:getMember() )
    dumpMessage(msg)

    conn:send( l2dbus.Message.newMethodReturn( msg ) )

    return l2dbus.Dbus.HANDLER_RESULT_HANDLED;

end -- defaultSvcHandler

----------------------------------------------------------------
--- execute
---
--- Execute some call over Dbus
---
--- @tparam   (string)  proxyObj  ....
--- @tparam   (string)  functName ....Name of DBus method to calll
--- @tparam   (string)  ...       ....args
---
--- @treturn  (boolean) true = success, else failure
--- @treturn  (string)  error on failure
----------------------------------------------------------------
local function execute( proxyObj, functName, ... )

    assert( proxyObj.proxy,     "ERROR: Bad proxy sent to execute" )
    assert( proxyObj.proxyCtrl, "ERROR: Bad proxyCtrl sent to execute" )

    local func = proxyObj.proxy.m[functName]
    if func == nil then
        local sErr = "ERROR: proxy does not contain function: "..functName or ""
        print( sErr )
        return false, sErr
    end

    local bOk, status, result = pcall(func, unpack(arg))  -- EXECUTE METHOD

    if bOk == false then
        print( "Error: ", status )
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

    print( "Result:" )

    if not status then
        print("Error: " .. result)
        print()
        return status, result

    elseif type(result) == "table" then
        -- Always a array because of getArgsAsArray
        local unpackedResult = unpack( result )
        if type(unpackedResult) == "table" then
            pretty.dump( unpackedResult )
        else
            print( unpackedResult )
        end
        print()
        return status, unpackedResult
    end

    print( result )
    print()

    return status, result

end -- execute

----------------------------------------------------------------
--- onAgentRequest
---
--- Generic DBus method handler
---
--- @tparam   (string)  prefix   ...."notify"|"sessionSvc"
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
---
--- @treturn  (number) l2dbus.Dbus.HANDLER_RESULT_NOT_YET_HANDLED |
---                    l2dbus.Dbus.HANDLER_RESULT_HANDLED
----------------------------------------------------------------
local function onAgentRequest(prefix, ifaceObj, conn, msg, userdata)

    local member  = msg:getMember()
    local methods = gSessionAPI

    print("\nonAgentRequest: " .. ifaceObj:name() .. "/" .. member, prefix )
    dumpMessage(msg)

    if prefix == "notify" then
        methods = gNotifyAPI
    end

    if methods[prefix.."_"..member] == nil then
        local sErr = string.format("ERROR: Unknown %s method: %s", prefix, member)
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "ERR_INV", sErr ) )
        return l2dbus.Dbus.HANDLER_RESULT_NOT_YET_HANDLED;
    end

    methods[prefix.."_"..member]( ifaceObj, conn, msg, userdata )

    return l2dbus.Dbus.HANDLER_RESULT_HANDLED;

end -- onAgentRequest

----------------------------------------------------------------
--- GetProxy
---
--- Helper to get a proxy object
---
--- @tparam   (string)  bus             ....system or session
--- @tparam   (table)   dbusAddr        ....array of: {busName, ObjPath, iface}
--- @tparam   (string)  objpathOverride ....optional override to
---                                         the dbusAddr object path
---
--- @treturn  (table) proxy and proxy controller object
----------------------------------------------------------------
local function GetProxy( bus, dbusAddr, objpathOverride )

    local proxyCtrl = proxyctrl.new(bus, dbusAddr[BUSNAME], objpathOverride or dbusAddr[OBJPATH] )
    assert(proxyCtrl:bind(), string.format( "ERROR: bind() to %s(%s) failed.",dbusAddr[BUSNAME], objpathOverride or dbusAddr[OBJPATH] ))
    local proxy     = proxyCtrl:getProxy( dbusAddr[IFACE] )
    assert( nil ~= proxy, string.format( "ERROR: getProxy() to %s(%s).%s failed.",dbusAddr[BUSNAME], objpathOverride or dbusAddr[OBJPATH], dbusAddr[IFACE] ) )
    return { proxy = proxy, proxyCtrl = proxyCtrl }

end -- GetProxy

----------------------------------------------------------------
--- InitDbus
---
--- Initialize parts of DBus
---
--- @tparam   (string)  bIsClient ....true, init as a client,
---                                   else, init as we are a service
----------------------------------------------------------------
local function InitDbus( bIsClient )

    if gSystemConn == nil then
        gSystemConn = l2dbus.Connection.openStandard(gDispatcher, l2dbus.Dbus.BUS_SYSTEM)
        assert( nil ~= gSystemConn )
    end

    if gSessionConn == nil then
        gSessionConn = l2dbus.Connection.openStandard(gDispatcher, l2dbus.Dbus.BUS_SESSION)
        assert( nil ~= gSessionConn )
    end

    if bIsClient == true then
        -- Setup proxy to connmantest-session service
        gSvcProxy = GetProxy( gSessionConn, SVC )
        return
    end

    -- Setup proxy to connman
    gMgrProxy = GetProxy( gSystemConn, CONNMAN_MGR )

end -- InitDbus

----------------------------------------------------------------
--- Notification
---
--- [add description here]
---
--- @tparam   (string)  notify_path ....
---
--- @treturn  (boolean) [add here]
----------------------------------------------------------------
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

----------------------------------------------------------------
--- TypeConvert
---
--- Converts specific configuration options to specific DBus
--- signature types.
---
--- @tparam   (string)  key   ....
--- @tparam   (any)     value ....
---
--- @treturn  return the actual DBus data type
----------------------------------------------------------------
local function TypeConvert( key, value )

    if key == "AllowedBearers" then -- comma separated
        return l2dbus.DbusTypes.Variant.new(utils.split( value, "," ), "vas")

    elseif key == "RoamingPolicy" or key == "ConnectionType" then
        return l2dbus.DbusTypes.Variant.new(value, "vs")

    elseif key == "Priority"      or key == "AvoidHandover" or
           key == "StayConnected" or key == "EmergencyCall" then
        local flag  = string.lower(value)
        local valid = { ["false"] = true, ["f"] = true, ["n"] = true, ["0"] = true }
        return not valid[ flag ]

    elseif key == "PeriodicConnect" or key == "IdleTimeout" then
        return l2dbus.DbusTypes.Uint32.new( tonumber(value) )
    end

    return value

end -- TypeConvert



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

    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- notify_Update



----------------------------------------------------------------
--- sessionSvc_CreateSession
---
--- Call to create a session.  This routine will:
--- 1. Register with the notification API for this session
--- 2. Create the session with the connman and store a proxy reference
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gSessionAPI.sessionSvc_CreateSession( ifaceObj, conn, msg, userdata )

    local margs = msg:getArgsAsArray()
    if margs == nil then
        local sErr = "ERROR: CreateSession: Missing argument 'session_name'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    local session_name = margs[1]
    local obj_path     = tostring(msg:getObjectPath())
    local sessionObj   = gSessionList[session_name]

    if sessionObj and sessionObj.session then
        local sErr = string.format( "Session %s already created -> dropped CreateSession request", session_name )
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    else
        sessionObj = {}
    end

    sessionObj.notify_path = obj_path .. "/" .. tostring( session_name )
    sessionObj.notify      = Notification( sessionObj.notify_path )
    if not sessionObj.settings then
        sessionObj.settings = {}
    end

    -- Reply now, so the next requests work
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

    local co = coroutine.create(function(  )
        local status, result = execute( gMgrProxy, "CreateSession", sessionObj.settings, sessionObj.notify_path )
        if status then
            sessionObj.session_path = result
        end
        print( "notify path: ", sessionObj.notify_path )
        print( "session path:", sessionObj.session_path )
        -- Setup the session interface to connman
        sessionObj.session = GetProxy( gSystemConn, CONNMAN_SESSION, sessionObj.session_path )
        gSessionList[session_name] = sessionObj
        end
    )
    local status, result = coroutine.resume(co)
    if not status then
        print("Error: " .. result)
    end

end -- sessionSvc_CreateSession

----------------------------------------------------------------
--- sessionSvc_Change
---
--- Change Passthru API
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
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gSessionAPI.sessionSvc_Change( ifaceObj, conn, msg, userdata )

    local margs = msg:getArgsAsArray()
    if margs == nil then
        local sErr = "ERROR: Change: Missing argument: 'session_name','key','value'"
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    local session_name = margs[1]
    local key          = margs[2]
    local value        = margs[3]

    if session_name == nil then
        local sErr = "ERROR: Change: Missing argument: 'session_name'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end
    if key == nil then
        local sErr = "ERROR: Change: Missing argument: 'key'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end
    if value == nil then
        local sErr = "ERROR: Change: Missing argument: 'value'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    local sessionObj = gSessionList[session_name]

    if sessionObj == nil or sessionObj.session == nil then
        local sErr = string.format( "Session %s is not running -> dropped Change request", session_name )
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    -- Reply now, so the next requests work
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

    local co = coroutine.create(function(  )
        execute( sessionObj.session, "Change", key, TypeConvert( key,value ) )
    end )
    local status, result = coroutine.resume(co)
    if not status then
        print("Error: " .. result)
    end

end -- sessionSvc_Change

----------------------------------------------------------------
--- sessionSvc_Configure
---
--- Configure API.  This is used prior to a create.  The settings
--- defined with this API will be directly used on the call to
--- the connman create routine.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gSessionAPI.sessionSvc_Configure( ifaceObj, conn, msg, userdata )

    local margs = msg:getArgsAsArray()
    if margs == nil then
        local sErr = "ERROR: Change: Missing argument: 'session_name','key','value'"
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    local session_name = margs[1]
    local key          = margs[2]
    local value        = margs[3]

    if session_name == nil then
        local sErr = "ERROR: Change: Missing argument: 'session_name'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end
    if key == nil then
        local sErr = "ERROR: Change: Missing argument: 'key'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end
    if value == nil then
        local sErr = "ERROR: Change: Missing argument: 'value'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    local sessionObj = gSessionList[session_name]

    if sessionObj == nil or sessionObj.session == nil then
        print( "Init new session..." )
        sessionObj = {}
        sessionObj.notify_path  = ""
        sessionObj.settings     = {}
        sessionObj.session_path = ""
        gSessionList[session_name] = sessionObj
    end

    if sessionObj and sessionObj.session then
        local sErr = string.format( "Session %s already runniing -> dropped Configure request", session_name )
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    sessionObj.settings[key] = TypeConvert( key, value )

    conn:send( l2dbus.Message.newMethodReturn( msg ) )

end -- sessionSvc_Configure

----------------------------------------------------------------
--- sessionSvc_Connect
---
--- Connect Passthru API
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
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gSessionAPI.sessionSvc_Connect( ifaceObj, conn, msg, userdata )

    local margs = msg:getArgsAsArray()
    if margs == nil then
        local sErr = "ERROR: Change: Missing argument: 'session_name'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    local session_name = margs[1]
    local sessionObj   = gSessionList[session_name]

    if sessionObj == nil or sessionObj.session == nil then
        local sErr = string.format( "Session %s is not running -> dropped Connect request", session_name )
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    -- Reply now, so the next requests work
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

    local co = coroutine.create(function(  )
        execute( sessionObj.session, "Connect" )
    end )
    local status, result = coroutine.resume(co)
    if not status then
        print("Error: " .. result)
    end

end -- sessionSvc_Connect

----------------------------------------------------------------
--- sessionSvc_DestroySession
---
--- Destroy Passthru API
---
--- Close the current session. This is similar to
--- DestroySession method on the manager interface. It
--- is just provided for convenience depending on how
--- the application wants to track the session.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gSessionAPI.sessionSvc_DestroySession( ifaceObj, conn, msg, userdata )

    local margs = msg:getArgsAsArray()
    if margs == nil then
        local sErr = "ERROR: Change: Missing argument: 'session_name'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    local session_name = margs[1]
    local sessionObj   = gSessionList[session_name]

    if sessionObj == nil or sessionObj.session == nil then
        local sErr = string.format( "Session %s is not running -> dropped Destroy request", session_name )
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    -- Reply now, so the next requests work
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

    local co = coroutine.create(function(  )
        execute( sessionObj.session, "Destroy" )
        sessionObj.session = nil

        -- "remove_from_connection" is part of test-session
        -- but has no handler in the connman.
        --execute( sessionObj.notify,  "remove_from_connection" )
        sessionObj.notify  = nil

        gSessionList[session_name] = nil
    end )
    local status, result = coroutine.resume(co)
    if not status then
        print("Error: " .. result)
    end

end -- sessionSvc_DestroySession

----------------------------------------------------------------
--- sessionSvc_Disconnect
---
--- Disconnect Passthru API
---
--- This method indicates that the current session does
--- not need a connection anymore.
---
--- This method returns immediately. The application is
--- informed through the update notification about the
--- state of the session.
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gSessionAPI.sessionSvc_Disconnect( ifaceObj, conn, msg, userdata )

    local margs = msg:getArgsAsArray()
    if margs == nil then
        local sErr = "ERROR: Change: Missing argument: 'session_name'"
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    local session_name = margs[1]
    local sessionObj   = gSessionList[session_name]

    if sessionObj == nil or sessionObj.session == nil then
        local sErr = string.format( "Session %s is not running -> dropped Disconnect request", session_name )
        print( sErr )
        conn:send( l2dbus.Message.newError( msg, "net.connman.Error.Failed", sErr ) )
        return
    end

    -- Reply now, so the next requests work
    conn:send( l2dbus.Message.newMethodReturn( msg ) )

    local co = coroutine.create(function(  )
        execute( sessionObj.session, "Disconnect" )
    end )
    local status, result = coroutine.resume(co)
    if not status then
        print("Error: " .. result)
    end

end -- sessionSvc_Disconnect

----------------------------------------------------------------
--- sessionSvc_Debug
---
--- Debug API to list the sessions tracked
---
--- @tparam   (string)  ifaceObj ....
--- @tparam   (string)  conn     ....
--- @tparam   (string)  msg      ....
--- @tparam   (string)  userdata ....
----------------------------------------------------------------
function gSessionAPI.sessionSvc_Debug( ifaceObj, conn, msg, userdata )

    local tRet = {}

    -- Debug info to return
    tRet.gSessionList = gSessionList

    local reply = l2dbus.Message.newMethodReturn( msg )

    reply:addArgs( pretty.write( tRet, " " ) )
    pretty.dump( tRet )

    conn:send( reply )

end -- sessionSvc_Debug


----------------------------------------------------------------
--- main
----------------------------------------------------------------
local function main()

    InitDbus() -- Init as a service

    if arg[1] == "enable" then
        execute( gMgrProxy, "SetProperty", "SessionMode",l2dbus.DbusTypes.Variant.new(true, "vb") )
        gDispatcher:stop()
        return

    elseif arg[1] == "disable" then
        execute( gMgrProxy, "SetProperty", "SessionMode",l2dbus.DbusTypes.Variant.new(false, "vb") )
        gDispatcher:stop()
        return

    end

    if arg[2] == nil then
        print( "Need test application path (e.g. '/foo')" )
        gDispatcher:stop()
        return
    end

    local app_path = arg[2]
    local app_name = string.format(SVC[BUSNAME], string.gsub(app_path, "/", ""))
    SVC[BUSNAME] = app_name
    SVC[OBJPATH] = SVC[OBJPATH]..app_path

    print(  )
    print( "Service BusName: ", SVC[BUSNAME] )
    print( "Service ObjPath: ", SVC[OBJPATH] )
    print(  )

    if arg[1] == "run" then
        ------------------
        -- Run as service
        ------------------
        local msg = l2dbus.Message.newMethodCall({destination = l2dbus.Dbus.SERVICE_DBUS,
                                                  path        = l2dbus.Dbus.PATH_DBUS,
                                                  interface   = l2dbus.Dbus.INTERFACE_DBUS,
                                                  method      = "RequestName"})
        msg:addArgsBySignature("su", app_name, l2dbus.Dbus.NAME_FLAG_DO_NOT_QUEUE)

        print("Requesting a session bus name: " .. app_name)
        local reply, errName, errMsg = gSessionConn:sendWithReplyAndBlock( msg )
        if reply == nil then
            error("Request Failed =>  " .. tostring(errName) .. " : " .. tostring(errMsg))
        else
            local result = reply:getArgs()
            assert( result == l2dbus.Dbus.REQUEST_NAME_REPLY_PRIMARY_OWNER )
            print("Acquired name successfully on session bus")
        end

        gProxySvc = l2dbus.ServiceObject.new( SVC[OBJPATH],
                                              defaultSvcHandler,
                                              "DefaultHandler" )
        -- Introspection interface
        assert( gProxySvc:addInterface( l2dbus.Introspection.new() ) )

        -- Main Interface
        gProxyIface = l2dbus.Interface.new( SVC[IFACE],
                                            function (ifaceObj, conn, msg, userdata)
                                                return onAgentRequest( "sessionSvc", ifaceObj, conn, msg, userdata )
                                            end,
                                            "MainHandler" )
        gProxyIface:registerMethods( SVC_METHODS )
        assert( gProxySvc:addInterface( gProxyIface ) )

        assert( gSessionConn:registerServiceObject( gProxySvc ) )

        return

    end -- "run"

    ------------------
    -- Run as client
    ------------------
    InitDbus( true ) -- init as client


    if arg[1] == "debug" then
        execute( gSvcProxy, "Debug" )
        gDispatcher:stop()
        return
    end

    if arg[3] == nil then
        print( "Need session name (e.g. 'sessionName')" )
        gDispatcher:stop()
        return
    end

    if arg[1] == "create" then
        execute( gSvcProxy, "CreateSession", arg[3] )

    elseif arg[1] == "destroy" then
        execute( gSvcProxy, "DestroySession", arg[3] )

    elseif arg[1] == "connect" then
        execute( gSvcProxy, "Connect", arg[3] )

    elseif arg[1] == "disconnect" then
        execute( gSvcProxy, "Disconnect", arg[3] )

    elseif arg[1] == "change" then

        if arg[4] == nil or arg[5] == nil then
            print( "Arguments missing" )
            gDispatcher:stop()
            return
        end
        execute( gSvcProxy, "Change", arg[3], arg[4], arg[5] )

    elseif arg[1] == "configure" then

        if arg[4] == nil or arg[5] == nil then
            print( "Arguments missing" )
            gDispatcher:stop()
            return
        end
        execute( gSvcProxy, "Configure", arg[3], arg[4], arg[5] )

    else
        print( "ERROR: Unknown command: ", arg[1] )

    end

    gDispatcher:stop()

end -- main


if arg[1] == nil then
    print( string.format("Usage: %s <command>", arg[0]))
    print( "" )
    print( "  enable" )
    print( "  disable" )
    print( "  debug     (use to list session info)" )
    print( "  create     <app_path> <session_name>" )
    print( "  destroy    <app_path> <session_name>" )
    print( "  connect    <app_path> <session_name>" )
    print( "  disconnect <app_path> <session_name>" )
    print( "  change     <app_path> <session_name> <AllowedBearers|ConnectionType> <value>" )
    print( "  configure  <app_path> <session_name> <key> <value>" )
    print( "" )
    print( "  run <app_path>" )
    print( "" )
    print( "Example Usage:" )
    print( "--------------" )
    print( "Service Side Process:" )
    print( "lua "..arg[0].." enable" )
    print( "lua "..arg[0].." run /foo" )
    print( "" )
    print( "Client/App Side Process: (different console)" )
    print( "Optionally configure the session:" )
    print( "(AllowedBearers, RoamingPolicy, ConnectionType, Priority, AvoidHandover, StayConnected, EmergencyCall, PeriodicConnect, IdleTimeout)" )
    print( "AllowedBearers==[ethernet|wifi|bluetooth|cellular|vpn|*] (comma separate)" )
    print( "ConnectionType==[any|local|internet]" )
    print( "lua "..arg[0].." configure /foo sessionName AllowedBearers \"*\"" )
    print( "Now create/connect the session:" )
    print( "lua "..arg[0].." create /foo sessionName" )
    print( "lua "..arg[0].." connect /foo sessionName" )
    print( "lua "..arg[0].." change /foo sessionName AllowedBearers \"ethernet,bluetooth\"" )
    print( "lua "..arg[0].." change /foo sessionName ConnectionType any" )
    print( "" )
    os.exit( 0 )
end

local timer = l2dbus.Timeout.new(gDispatcher, 25, false, function()
                    local co = coroutine.create(main)
                    local status, result = coroutine.resume(co)
                    if not status then
                        print("Error: " .. result)
                    end
              end)
timer:setEnable(true)

gDispatcher:run( l2dbus.Dispatcher.DISPATCH_WAIT )

gSessionConn = nil
gSystemConn  = nil
gMgrProxy    = nil

l2dbus.shutdown()

