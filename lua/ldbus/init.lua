--[[
*****************************************************************************

Project         ldbus

Released under the MIT License (MIT)
Copyright (c) 2013 XS-Embedded LLC

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

*****************************************************************************
*****************************************************************************
@file           init.lua
@author         Glenn Schmottlach
@brief          The core (low-level) ldbus module.
*****************************************************************************
--]]

--- A shim for the legacy Lua binding to the XSe ServiceProvider D-Bus interface.
-- This module provides both an adaptor (service) and proxy
-- (client) interface. All calls to the API should be made from within the
-- context of a Lua coroutine rather than the *main* coroutine (or thread).
-- This module utilizes the underlying services of L2DBUS to implement the
-- XSe ServiceProvider interface.
-- @module ldbus
-- @author Glenn Schmottlach
-- @copyright 2013 XS-Embedded LLC
-- @license MIT License (MIT)


local l2dbus = require("l2dbus")
local service = require("l2dbus.service")
local proxyCtrl = require("l2dbus.proxyctrl")
local msgBusCtrl = require("l2dbus.msgbusctrl")
local validate = require("l2dbus.validate")
local logging = require("logging")
local ev = require("ev")


local M = { }
--- Module version
local VERSION = "1.2.1"

-- Filled in by the init() routine
local mainLoop
local dispatch
local loopType
local evLoop

local MATCH_ALL_BUSNAMES = ".all_names"

--- XS Embedded Service Provider Interface Description
local XS_EMBEDDED_SERVICE_PROVIDER_INF = "com.xsembedded.ServiceProvider"
local XS_EMBEDDED_SERVICE_PROVIDER_XML =
[[
<!DOCTYPE node PUBLIC "­//freedesktop//DTD D­BUS Object Introspection 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
    <interface name="com.xsembedded.ServiceProvider">
        <method name="Request">
            <arg name="methodName" direction="in" type="s"/>
            <arg name="arguments" direction="in" type="s"/>
            <arg name="result" direction="out" type="s"/>
        </method>
        <signal name="Notify">
            <arg name="signalName" direction="out" type="s"/>
            <arg name="payload" direction="out" type="s"/>
        </signal>
    </interface>
</node>
]]

--- Error Information Table
-- - contains error information associated with selected method calls.
-- @class table
-- @name ErrorInfo
-- @field errCode Contains a numerical error code (one of the constant ERR_XXXXX codes)
-- @field errMsg An optional error message associated with the error code

--- The D-Bus "system" bus.
M.SYSTEM_BUS = l2dbus.Dbus.BUS_SYSTEM

--- The D-Bus "session" bus.
M.SESSION_BUS = l2dbus.Dbus.BUS_SESSION

--- The D-Bus "activation" bus used to start services.
M.ACTIVATION_BUS = l2dbus.Dbus.BUS_STARTER

--- The default error name used in D-Bus error messages.
M.DEFAULT_ERROR_NAME = "com.xsembedded.ServiceProvider.Error"

--
-- Known error codes emitted from ldbus core.
--

--- Error code for no error (everything "ok")
M.ERR_OK = 0

-- Error code indicating a D-Bus error detected
M.ERR_DBUS = 1

--- Error code indicating failure to submit a command to the D-Bus layer.
M.ERR_CMD_SUBMISSION = 2

--- Error code indicating the D-Bus bus type was unrecognized
M.ERR_UNKNOWN_BUS = 3

--- Error code indicating an unknown adaptor was specified
M.ERR_UNKNOWN_ADAPTOR = 4

--- Error code indicating an unknown proxy was specified
M.ERR_UNKNOWN_PROXY = 5

--- Error code indicating a failure to register a proxy or adaptor
M.ERR_REGISTRATION = 6

--- Error code indicating a Lua "error" was caught
M.ERR_LUA_ERROR = 7

--- Error code indicating an undefined/unknown error occurred
M.ERR_UNDEFINED = 8


-- Module logging facility.
local log = logging.new(function(self, level, message)
    local outMsg = logging.prepareLogMsg("%level %message\n", os.date(), level, message)
    if (level == logging.WARN) or (level == logging.ERROR) or
        (level == logging.FATAL) then
        io.stderr:write(outMsg)
        io.stderr:flush()
    else
        io.stdout:write(outMsg)
        io.stdout:flush()
    end
    return true
end)

-- By default we're only interested and WARNING and worse logs
log:setLevel(logging.WARN)

--- Collects arguments into an array with a single field 'n' containing the length.
-- @tparam array ... An array of arguments
-- @treturn array An array of arguments with the number of args in field 'n'.
function M.tuple(...)
  return {n=select('#', ...), ...}
end


--- Strips the leading/trailing whitespace characters from a string.
-- @tparam string s The string to strip
-- @treturn string A string with the leading/trailing whitespace removed.
function M.strip(s)
    return s:match("^%s*(.-)%s*$")
end



--- Verifies the given name is a valid D-Bus well-known bus name.
-- Note: this function does *not* validate private bus names.
-- @tparam string name Well-known bus name to check
-- @treturn boolean True if well-known bus name is valid, false otherwise.
M.isValidWellKnownBusName = validate.isValidBusName


--- Verifies the given name is a valid D-Bus object path.
-- @tparam string name The object path to check
-- @treturn boolean True if the object name is valid, false otherwise.
M.isValidObjectPath = validate.isValidObjectPath


local function newProxy(bus, busName, objPath)
    local self = {
                proxyCtrl = nil,
                sigMap = {},
                globalSignalHandler = nil,
                }

    --- Proxy.
    -- A D-Bus proxy/client class.
    -- @section Proxy

    --- Get the "id" of the proxy.
    -- This is an opaque value representing the underlying (and hidden) proxy object.
    -- @treturn lightuserdata An opaque identifier for the underlying object.
    local id = function()
        local addr = string.gsub(tostring(self.proxyCtrl), "userdata:%s*", "")
        return tonumber(addr)
    end

    --- Make a request of a D-Bus service.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- @tparam string method The name of the method to call
    -- @tparam string param The parameter string
    -- @tparam boolean ignoreReply Set to 'true' if function is not
    -- interested in a reply (and hence does not block waiting for one).
    -- If 'false' or nil the function should block waiting for a reply.
    -- @treturn nil (on failure) or returned response encoded as a string.
    -- If the response is being ignored then 'true' is returned on
    -- successful submission of request or nil on failure.
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local request = function(method, param, ignoreReply)
        self.proxyCtrl:setProxyNoReplyNeeded(ignoreReply == true)
        local proxy = self.proxyCtrl:getProxy(XS_EMBEDDED_SERVICE_PROVIDER_INF)
        local status, pending, errMsg = proxy.m.Request(method, param)
        if status then
            -- If we're running in the main thread then ...
            local co = coroutine.running()

            -- No sense waiting for a reply we don't care about
            if ignoreReply then
                return true, {errCode = M.ERR_OK, errMsg = ""}
            elseif co == nil then
                -- We're in the main thread - the only thing we can do
                -- is block and wait for a response.
                pending:block()
                local msg = pending:stealReply()
                if msg:getType() == l2dbus.Dbus.MESSAGE_TYPE_ERROR then
                    return nil, {errCode = M.ERR_DBUS, errMsg = tostring(msg:getArgs()) }
                else
                    return msg:getArgs(), {errCode = M.ERR_OK, errMsg = ""}
                end
            -- Else we're running in a coroutine already
            else
                pending:setNotify(function(p, token)
                    local msg = p:stealReply()
                    if msg:getType() == l2dbus.Dbus.MESSAGE_TYPE_ERROR then
                        coroutine.resume(token, nil, {errCode = M.ERR_DBUS,
                                        errMsg = tostring(msg:getArgs())})
                    else
                        coroutine.resume(token, msg:getArgs(),
                                        {errCode = M.ERR_OK, errMsg = "" })
                    end
                end, co)
                -- Yield the coroutine until we get an answer
                return coroutine.yield()
            end
        else
            return nil, {errCode = M.ERR_DBUS, errMsg = errMsg }
        end
    end


    --- Subscribe to the named signal.
    -- @tparam string sigName The name of the signal to subscribe. If 'nil' then
    -- the handler specifies a global handler that will receive all signals
    -- for this proxy.
    -- @tparam function handler The function to receive the signal notification.
    -- The handler will receive the signal name (string) and a parameter string.
    -- @return True on success otherwise Lua error() is called
    local subscribe = function(sigName, handler)
        assert( (type(sigName) == "nil") or ((type(sigName) == "string") and (#sigName > 0)) )
        assert( (type(handler) == "function") or (type(handler) == "thread") )
        if sigName then
            self.sigMap[sigName] = handler
        else
            self.globalSignalHandler = handler
        end
        return true
    end

    --- Unsubscribe from the named signal.
    -- @tparam string The name of the signal to unsubscribe. If 'nil' then
    -- the global signal handler is unsubscribed.
    -- @return True on success otherwise Lua error() is called
    local unsubscribe = function(sigName)
        assert( (type(sigName) == "string") or (type(sigName) == "nil") )
        if sigName then
            self.sigMap[sigName] = nil
        else
            self.globalSignalHandler = nil
        end
        return true
    end


    -- (Private) Processes all signals directed to this proxy
    local handleSignal = function(sigName, payload)
        local handler = self.globalSignalHandler or self.sigMap[sigName]
        if handler ~= nil then
            local co = handler
            if type(handler) == "function" then
                co = coroutine.create(handler)
            end
            -- Run the handler within the context of a coroutine
            local status, result = coroutine.resume(co, sigName, payload)

            -- If the coroutine threw an error while running then ...
            if status == false then
                log:error("Error thrown handling signal <" .. sigName .. ">: "
                            .. tostring(result))
            end
        end
    end

    -- (Private) Returns the internal proxy userdata
    local private = function()
       return self.proxyCtrl
    end

    -- (Private) Destructor for the proxy
    local destroy = function()
        self.proxyCtrl:disconnectAllSignals()
        self.proxyCtrl:unbind()
        return "", {errCode = M.ERR_OK, errMsg = ""}
    end

    --
    -- Proxy constructor
    --

    if M.isValidObjectPath(objPath) and M.isValidWellKnownBusName(busName) then
        local config = function()
            local pCtrl = proxyCtrl.new(bus, busName, objPath)
            pCtrl:bindNoIntrospect(XS_EMBEDDED_SERVICE_PROVIDER_XML)
            pCtrl:setBlockingMode(false)
            pCtrl:connectSignal(XS_EMBEDDED_SERVICE_PROVIDER_INF, "Notify",
                                handleSignal)
            return pCtrl
        end

        local status, value = pcall(config)
        if status then
            self.proxyCtrl = value
        end
    end

    if not self.proxyCtrl then
        return nil
    else
        return {
            id = id,
            request = request,
            subscribe = subscribe,
            unsubscribe = unsubscribe,

            -- Private (internal) methods
            handleSignal = handleSignal,
            private = private,
            destroy = destroy
        }
    end
end


local function newAdaptor(objPath, b)
    local self = {
                adaptor = nil,
                bus = b,
                methodMap = {},
                globalMethodHandler = nil,
                }

    --- Adaptor.
    -- A D-Bus adaptor/service class.
    -- @section Adaptor

    --- Get the "id" of the adaptor.
    -- This is an opaque value representing the underlying (and hidden) adaptor object.
    -- @treturn lightuserdata An opaque identifier for the underlying object.
    local id = function()
        local addr = string.gsub(tostring(self.adaptor), "userdata:%s*", "")
        return tonumber(addr)
    end

    --- Reply to a client's request.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- @tparam userdata context The context under which to deliver the reply
    -- @tparam string response The response encoded as a string
    -- @treturn nil (on failure) or an empty string acknowledging the reply has been sent
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local reply = function(context, response)
        if context:reply(response) then
            return "", {errCode=M.ERR_OK, errMsg=""}
        else
            return nil, {errCode=M.ERR_DBUS, errMsg="Failed to reply"}
        end
    end

    --- Sends a signal to subscribed listeners.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- @tparam string sigName The name of the signal to emit
    -- @tparam string param The data associated with the signal encoded as a string
    -- @treturn nil (on failure) or an empty string acknowledging the notification has been sent
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local notify = function(sigName, param)
        if self.adaptor:emit(self.bus, XS_EMBEDDED_SERVICE_PROVIDER_INF,
            "Notify", sigName, param) then
            return "", {errCode = M.ERR_OK, errMsg = ""}
        else
            return nil, {errCode = M.ERR_DBUS, errMsg = "Failed to end notification"}
        end
    end


    --- Sends a D-Bus error message.
    -- A valid response to a client request is to either send a reply
    -- or send a D-Bus error message. Only *one* action should be taken (not both).
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- @tparam userdata context The context under which to deliver the error
    -- @tparam string errName The name of the error message. If nil then the default error
    -- message name is used.
    -- @tparam string errMsg The (optional) message associated with the error
    -- @treturn nil (on failure) or an empty string acknowledging the error has been sent
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local dbusError = function(context, errName, errMsg)
        errName = errName or M.DEFAULT_ERROR_NAME

        if context:error(errName, errMsg) then
            return "", {errCode = M.ERR_OK, errMsg = ""}
        else
            return nil, {errCode = M.ERR_DBUS, errMsg = "Failed to emit error"}
        end
    end


    -- (Private) Sends a D-Bus error message from a pcall.
    local function emitError(context, msg)
        return dbusError(context, M.DEFAULT_ERROR_NAME, msg)
    end

    -- (Private) Returns the internal userdata adaptor.
    local private = function()
       return self.adaptor
    end


    -- (Private) Handles all method requests for this adaptor
    local handleRequest = function(ctx, method, payload)
        local handler = self.globalMethodHandler or self.methodMap[method]
        if handler == nil then
            -- Make best effort to send the D-Bus error message
            emitError(ctx, "No request handler found")
        else
            local co = handler
            if type(handler) == "function" then
                co = coroutine.create(handler)
            end
            -- Run the handler within the context of a coroutine
            local msg = ctx:getMessage()
            local status, result = coroutine.resume(co, ctx, method, payload,
                                                    msg:getSender(),
                                                    msg:getSerial())

            -- If the coroutine threw an error while running then ...
            if status == false then
                log:warn("Handler for '" .. method .. "' failed: " .. tostring(result))
                emitError(ctx, "Handler for '" .. method .. "' failed: " .. tostring(result))
            end
        end
    end


    --- Registers a function to handle requests for a particular method
    -- @tparam string method The method name being registered. If 'nil' then
    -- this handler will receive *all* method requests.
    -- @tparam function handler The function which will handle replying to the request
    -- @return True on success otherwise Lua error() is called
    local registerMethod = function(method, handler)
        assert( (type(method) == "nil") or ((type(method) == "string") and (#method > 0)) )
        assert( (type(handler) == "function") or (type(handler) == "thread") )
        if method ~= nil then
            self.methodMap[method] = handler
        else
            self.globalMethodHandler = handler
        end
        return true
    end


    --- Unregisters a method from being handled by the adaptor
    -- @tparam string method The method name being unregistered. If 'nil' then
    -- any global method handler is unregistered.
    -- @return True on success otherwise Lua error() is called
    local unregisterMethod = function(method)
        assert( (type(method) == "string") or (type(method) == nil) )
        if method then
            self.methodMap[method] = nil
        else
            self.globalMethodHandler = nil
        end

        return true
    end


    -- (private) Destructor for adaptor
    local destroy = function()
        local dtor = function()
            self.adaptor:unregisterMethodHandler(XS_EMBEDDED_SERVICE_PROVIDER_INF,
                                                "Request")
            self.adaptor:removeInterface(XS_EMBEDDED_SERVICE_PROVIDER_INF)
            self.adaptor:detach(self.bus)
        end

        local status, result = pcall(dtor)
        if not status then
            return nil, {errCode = M.ERR_LUA_ERROR, errMsg = result}
        else
            return "", {errCode = M.ERR_OK, errMsg = ""}
        end
    end

    --
    -- Adaptor constructor
    --

    local result = nil
    local status = false

    if M.isValidObjectPath(objPath) then
        status, result = pcall(service.new, objPath, true, nil)
        if status then
            self.adaptor = result
            status, result = pcall(self.adaptor.attach, self.adaptor, self.bus)
            if not status then
                self.adaptor = nil
            else
                status, result = pcall(self.adaptor.addInterface, self.adaptor,
                                        XS_EMBEDDED_SERVICE_PROVIDER_INF,
                                        XS_EMBEDDED_SERVICE_PROVIDER_XML)
                if not status then
                    self.adaptor:detach(self.bus)
                    self.adaptor = nil
                else
                    -- Register the method handler. This will receive all
                    -- ServiceProvider methods
                    status, result = pcall(self.adaptor.registerMethodHandler,
                                        self.adaptor,
                                        XS_EMBEDDED_SERVICE_PROVIDER_INF,
                                        "Request", handleRequest)
                    if not status then
                        self.adaptor:removeInterface(XS_EMBEDDED_SERVICE_PROVIDER_INF)
                        self.adaptor:detach(self.bus)
                        self.adaptor = nil
                    end
                end
            end
        end
    end

    if not self.adaptor then
        return nil
    else
        return {
            id = id,
            reply = reply,
            notify = notify,
            dbusError = dbusError,

            -- Private (internal) methods
            handleRequest = handleRequest,
            registerMethod = registerMethod,
            unregisterMethod = unregisterMethod,
            private = private,
            destroy = destroy
            }
    end
end


local function newBus(b)
    local self = {
        bus = b,
        sigMap = {},
        msgBusCtrl = nil,
        sigHnd = nil
        }

    --- Bus.
    -- A D-Bus Bus class.
    -- @section Bus

    --- Closes an open D-Bus bus.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- @treturn nil (on failure) or an empty string indicating the bus is closed
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local close = function()
        self.msgBusCtrl:disconnectAllSignals()
        self.msgBusCtrl:unbind()
        self.msgBusCtrl = nil
        if self.bus:isConnected() then
            self.bus:flush()
        end
        self.bus = nil
        return "", {errCode = M.ERR_OK, errMsg = ""}
    end

    --- Checks to see whether the given well-known D-Bus bus name has an owner.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- @tparam string name The well-known D-Bus name to check for an owner
    -- @treturn nil (on failure) or a table containing { owner = true/false }
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local hasName = function(name)
        local result = nil
        local errInfo = {errCode = M.ERR_LUA_ERROR,
                        errMsg = "Invalid well-known D-Bus bus name"}
        if M.isValidWellKnownBusName(name) then
            local msgBus = self.msgBusCtrl:getProxy()
            local status, value, errMsg = msgBus.m.NameHasOwner(name)
            if status then
                result = {owner = value}
                errInfo = {errCode = M.ERR_OK, errMsg = ""}
            else
                errInfo = {errCode = M.ERR_DBUS,
                            errMsg = string.format("%s : %s", value, errMsg)}
            end
        end

        return result, errInfo
    end


    --- Requests the well-known D-Bus name from the D-Bus daemon.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- @tparam string name The well-known D-Bus bus name being requested
    -- @tparam number busFlags D-Bus flags indicating the ownership
    -- options. By default (if nil is specified) then DBUS_NAME_FLAG_DO_NOT_QUEUE
    -- is used. Possible options include: DBUS_NAME_FLAG_DO_NOT_QUEUE, DBUS_NAME_FLAG_REPLACE_EXISTING,
    -- or DBUS_NAME_FLAG_ALLOW_REPLACEMENT.
    -- @treturn nil (on failure) or an empty string on success
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local requestName = function(name, busFlags)
        local result = nil
        local errInfo = {errCode = M.ERR_LUA_ERROR,
                        errMsg="Invalid well-known D-Bus bus name"}

        if busFlags == nil then
            busFlags = l2dbus.Dbus.NAME_FLAG_DO_NOT_QUEUE
        end

        if M.isValidWellKnownBusName(name) then
            local msgBus = self.msgBusCtrl:getProxy()
            local status, value, errMsg = msgBus.m.RequestName(name, busFlags)
            if not status then
                errInfo = {errCode = M.ERR_DBUS,
                            errMsg = string.format("%s : %s", value, errMsg)}
            else
                if (value == l2dbus.Dbus.REQUEST_NAME_REPLY_PRIMARY_OWNER) or
                    (value == l2dbus.Dbus.REQUEST_NAME_REPLY_ALREADY_OWNER) then
                    result = ""
                    errInfo = {errCode = M.ERR_OK, errMsg = ""}
                else
                    errInfo = {errCode = M.ERR_DBUS,
                                errMsg = string.format("Failed to acquire D-Bus name (code=%d)", value)}
                end
            end
        end

        return result, errInfo
    end


    --- Creates a new adaptor with the given D-Bus object path.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- @tparam string objPath The D-Bus object path to associate with the adaptor
    -- @treturn nil (on failure) or a new adaptor object
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local newAdaptor = function(objPath)
        local errInfo = {errCode = M.ERR_LUA_ERROR,
                        errMsg = ""}
        local adaptor = newAdaptor(objPath, self.bus)
        if not adaptor then
            errInfo.errMsg = "Failed to create adaptor"
            return nil, errInfo
        else
            errInfo.errCode = M.ERR_OK
            return adaptor, errInfo
        end
    end


    --- Destroys or deallocates the specific adaptor.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- The adaptor is no longer valid after this call and no
    -- methods should be called on it.
    -- @tparam table The adaptor to deallocate and destroy
    -- @treturn nil (on failure) or an empty string on success
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local destroyAdaptor = function(adaptor)
        return adaptor.destroy()
    end


    --- Creates a new proxy for the specifed object registered under the given bus name.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- @tparam string busName The well-known D-Bus bus name the object is registered under
    -- @tparam string objPath The D-Bus object path of the proxied service
    -- @treturn nil (on failure) or a new proxy object
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local newProxy = function(busName, objPath)
        local errInfo = {errCode = M.ERR_LUA_ERROR, errMsg=""}
        local result = newProxy(self.bus, busName, objPath)
        if not result then
            errInfo.errMsg = "Failed to create proxy"
            return nil, errInfo
        else
            errInfo.errCode = M.ERR_OK
            return result, errInfo
        end
    end


    --- Destroys or deallocates the specific proxy.
    -- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
    -- The proxy is no longer valid after this call and no
    -- methods should be called on it.
    -- @tparam table The proxy to deallocate and destroy
    -- @treturn nil (on failure) or an empty string on success
    -- @return @{ErrorInfo} which is a table: {errCode, errMsg}
    local destroyProxy = function(proxy)
        return proxy.destroy()
    end


    --- Get the "id" of the bus.
    -- This is an opaque value representing the underlying (and hidden) bus object.
    -- @treturn lightuserdata An opaque identifier for the underlying object.
    local id = function()
        local addr = string.gsub(tostring(self.bus), "userdata:%s*", "")
        return tonumber(addr)
    end


    -- (Private) Handles and dispatches all signals recieved by this bus
    -- to the appropriate handler.
    local handleNameOwnerChanged = function(name, oldOwner, newOwner)
        for k,handler in pairs(self.sigMap) do
            if k == name or k == MATCH_ALL_BUSNAMES then
                local co = handler
                if type(handler) == "function" then
                    co = coroutine.create(handler)
                end
                -- Run the handler within the context of a coroutine
                local status, result = coroutine.resume(co, name,
                                                        oldOwner, newOwner)

                -- If the coroutine threw an error while running then ...
                if status == false then
                    log:error("Error thrown handling NameOwnerChanged <" ..
                                name .. ">: " .. tostring(result))
                end
            end
        end
    end


    --- Subscribes a function to handle NameOwnerChanged signals from the bus.
    -- @tparam string busName The D-Bus bus name that should be monitored. If nil is
    -- specified then *all* bus names are monitored.
    -- @tparam function handler The function which will receive notification updates. The
    -- handler should accept three arguments: name, oldOwner, and newOwner.
    -- @return True on success otherwise Lua error() is called
    local subscribeNameOwnerChanged = function(busName, handler)
        assert( type(busName) == "nil" or type(busName) == "string" )
        assert( (type(handler) == "function") or (type(handler) == "thread") )
        if busName == nil then
            -- ".all_names" is an illegal bus name
            self.sigMap[MATCH_ALL_BUSNAMES] = handler
        else
            assert(busName, "Invalid busname")
            self.sigMap[busName] = handler
        end
        return true
    end


    --- Unsubscribes handling NameOwnerChanged signals for a specific bus name.
    -- @tparam string busName The D-Bus bus name that we should stop monitoring
    -- for NameOwnerChanged signals from the D-Bus daemon. If 'nil' then the
    -- global handler is unsubscribed.
    -- @return True on success otherwise Lua error() is called
    local unsubscribeNameOwnerChanged = function(busName)
        assert( type(busName) == "nil" or type(busName) == "string" )
        if busName == nil then
            self.sigMap[MATCH_ALL_BUSNAMES] = nil
        else
            self.sigMap[busName] = nil
        end

        return true
    end

    --- Get the maximum number of bytes that can be used by all
    -- messages received on this connection.
    -- @return The maximum total number of bytes.
    local getMaxReceivedSize = function()
        return self.bus:getMaxReceivedSize()
    end

    --- Set the maximum number of bytes that can be used by all
    -- messages received on this connection.
    -- @tparam number size The maximum total number of bytes.
    local setMaxReceivedSize = function(size)
        self.bus:setMaxReceivedSize(size)
    end

    --- Get the approximate size in bytes of al messages in the outgoing queue.
    -- @tparam number size The maximum total number of bytes.
    local getOutgoingSize = function()
        return self.bus:getOutgoingSize()
    end

    -- (Private) Returns the underlying opaque handle to the bus
    local private = function()
        return self.bus
    end

    local busObj = {
        close = close,
        hasName = hasName,
        requestName = requestName,
        id = id,
        newAdaptor = newAdaptor,
        destroyAdaptor = destroyAdaptor,
        newProxy = newProxy,
        destroyProxy = destroyProxy,
        getMaxReceivedSize = getMaxReceivedSize,
        setMaxReceivedSize = setMaxReceivedSize,
        getOutgoingSize = getOutgoingSize,

        -- Private (internal) methods
        private = private,
        subscribeNameOwnerChanged = subscribeNameOwnerChanged,
        unsubscribeNameOwnerChanged = unsubscribeNameOwnerChanged,
        handleSignal = handleNameOwnerChanged,
        }

    -- Append D-Bus constants
    for k,v in pairs(l2dbus.Dbus) do
        local flag = string.match(k, "^NAME_FLAG.+")
        if flag ~= nil then
            busObj["DBUS_" .. flag] = l2dbus.Dbus[flag]
        end
    end

    -- Connect to the Message Bus
    self.msgBusCtrl = msgBusCtrl.new(self.bus)
    self.msgBusCtrl:bind(false)
    self.msgBusCtrl:setBlockingMode(true)
    self.sigHnd = self.msgBusCtrl:connectSignal("NameOwnerChanged",
                                            busObj.handleSignal)
    return busObj
end


--- Module.
-- Module scoped functions
-- @section Module

--- Creates and opens a D-Bus bus.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @param uri Either a raw D-Bus URI (string) or one of the well-known DBUS_SYSTEM_BUS,
-- DBUS_SESSION_BUS, or DBUS_ACTIVATION_BUS constants
-- @tparam boolean private If the URI is specified as a string then the bus can optionally
-- have a shared or private connection to the D-Bus daemon. If one of the well-known
-- bus names is used (SYSTEM, SESSION, or ACTIVATION) then this parameter is unused.
-- @treturn nil (on failure) or a bus object on success
-- @return @{ErrorInfo} which is a table: {errCode, errMsg}
function M.openBus(uri, private)
    local openFunc = nil
    if type(uri) == "number" then
        openFunc = l2dbus.Connection.openStandard
    else
        openFunc = l2dbus.Connection.open
    end

    if type(private) ~= "boolean" then
        private = private and true or false
    end

    local status, result = pcall(openFunc, dispatch, uri, private, false)
    if status then
        -- Wrap the connection
        local bus = newBus(result)
        return bus, {errCode = M.ERR_OK, errMsg = ""}
    else
        return nil, {errCode = M.ERR_DBUS, errMsg = result}
    end
end


--- Closes an open D-Bus bus.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @tparam table bus The bus to closed. After the bus is closed then
-- it should be considered deallocated and not further methods
-- should be called on it.
-- @treturn nil (on failure) or an empty string on success
-- @return @{ErrorInfo} which is a table: {errCode, errMsg}
function M.closeBus(bus)
    bus.close()
    return "", {errCode = M.ERR_OK, errMsg = ""}
end

--- Gets the underlying libev loop used by the module.
-- @return Libev main loop
function M.getLoop()
    if mainLoop == nil then
        -- For backward compatibility, initialize for a libev mainloop
        M.init( "l2dbus_ev" )
    end

    if loopType == "l2dbus_ev" then
        return evLoop
    else
        -- Only useful for libev loops, so return nil
        return nil
    end
end

--- Gets the underlying L2DBUS dispatcher for the shim.
-- @return L2DBUS dispatcher
function M.getDispatcher()
    return dispatch
end


--- Start the dispatcher
-- @return None
function M.startDispatch()
    -- Do nothing
end


--- Stops the main loop for the module.
-- If there are no other event loop libev "watchers" (e.g. timers, IO handlers, etc..)
-- then the call to "loop" will exit.
-- @return None
function M.stopLoop()
    dispatch:stop()
end

--- Selects and inits the main loop for the module.
-- If this routine is not called prior to loop() libev
-- will be intialized for backward compatibility.
-- @param sLoopType the type of loop to init
-- ("l2dbus_ev", "l2dbus_glib", ...)
-- @return None
function M.init( sLoopType )

    local initLoop

    loopType = sLoopType or "l2dbus_ev"

    if sLoopType == "l2dbus_ev" then

        evLoop = ev.Loop.default
        -- Call a method to force the loop to actually be allocated
        evLoop:now()
        initLoop = evLoop

    elseif sLoopType == "l2dbus_glib" then

        -- NOP, ensures sLoopType is supported
        --      uses the generic init below

    else
        assert( 0, "ERROR: Unknown l2dbus loop type on init(): "..sLoopType )
    end

    mainLoop = require(sLoopType).MainLoop.new(initLoop)
    dispatch = l2dbus.Dispatcher.new(mainLoop)
end

--- Main loop for the module.
-- This function does *not* return
-- @param init Either an initialization routine to run as a function (wrapped in a coroutine)
-- or resumed directly if already a coroutine.
-- @tparam array ... An array of initial arguments to be provided to the initialization function
-- @return None
function M.loop(init, ...)

    -- - - - - - - - - - - - - - - - - - - - - - - - - -
    local function startup()
        local status, result = nil
        if type(init) == "function" then
            local co = coroutine.create(init)
            status, result = coroutine.resume(co, initArgs)
        elseif type(init) == "thread" then
            status, result = coroutine.resume(init, initArgs)
        end
        if not status then
            log:warn("Init function error: " .. result)
            M.stopLoop()
        end
        return status, result
    end
    -- - - - - - - - - - - - - - - - - - - - - - - - - -

    if mainLoop == nil then
        -- For backward compatibility, initialize for a libev mainloop
        M.init( "l2dbus_ev" )
    end

    local initArgs = ...
    if (type(init) ~= "function") and (type(init) ~= "thread") and
        (type(init) ~= nil) then
        error("First parameter must be a function, coroutine, or nil")
    end

    -- If using libev take advantage of its main loop,
    -- otherwise we will use a timer to put us in a
    -- coroutine to initialize.
    if evLoop then
        local starter = ev.Idle.new(function(loop, idle, revents)
                idle:stop(loop)
                startup()
            end)
        starter:start(evLoop)
    else
        -- Use a short l2dbus timer to get us in a coroutine.
        local timeout = l2dbus.Timeout.new(dispatch, 1, false, function() startup() end )
        timeout:setEnable( true )
    end

    -- Loop forever processing events
    dispatch:run(l2dbus.Dispatcher.DISPATCH_WAIT)
end

--- Iterate the D-Bus event handler once
-- @return None
function M.loopIterate(runOpt)
    if nil == runOpt then
        dispatch:run(l2dbus.Dispatcher.DISPATCH_NO_WAIT)
    else
        dispatch:run(runOpt)
    end
end


-- Called when this module is run as a program
local function main(arg)
    print(string.match(arg[0], "^(.+)%.lua") .. " - Version: " .. VERSION)
end


-- Determine the context in which the module is usd
if require('l2dbus.is_main')() then
    -- The module is being run as a program
    main(arg)
else
    -- The module is being loaded rather than run
    return M
end
