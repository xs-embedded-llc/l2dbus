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
@file           proxy.lua
@author         Glenn Schmottlach
@brief          Proxy helper class that utilizes legacy ldbus module.
*****************************************************************************
--]]

--- XSe ServiceProvider Proxy Helper Class
-- This module wraps much of the proxy-oriented functionality of
-- the ldbus module and exposes a friendlier API packaged as
-- a directly instantiatable class (Proxy). This class serves
-- as a client proxy to another service that implements the
-- com.xsembedded.ServiceProvider D-Bus interface.
-- @module proxy
-- @alias M
-- @author Glenn Schmottlach
-- @copyright 2013 XS-Embedded LLC
-- @license MIT License (MIT)

local ldbus = require("ldbus")
local cjson = require("cjson.safe")
local class = require("pl.class")

--- Module version
local VERSION = "1.0.0"

local M = {}


-- Returns an "errorInfo" table. By default (called with
-- no parameters) it returns a table indicating no error.
local function makeErrInfo(code, msg)
    if type(code) ~= "number" then
        code = ldbus.ERR_OK
    end
    
    if type(msg) ~= "string" then
        msg = ""
    end
    return { errCode = code, errMsg = msg}
end


-- A variant of the Lua "assert()" that doesn't call error().
-- It returns an @{ldbus.ErrorInfo} object if 'v' evaluates to false (or nil).
-- If 'v' is true then nil is returned.
local function verify(v, msg)
    if not v then
        return makeErrInfo(ldbus.ERR_LUA_ERROR, msg)
    else
        return nil
    end
end


--- A D-Bus com.xsembedded.ServiceProvider proxy class
-- @type Proxy
M.Proxy = class()


--- Contructor for the Proxy class.
-- @tparam string busName The well-known D-Bus bus name this proxy communicates with
-- @tparam string objectPath The D-Bus object path associated with this proxy
-- @return None
function M.Proxy:_init(busName, objectPath)
    assert(type(busName) == "string", "D-Bus well known bus name must be specified")
    assert(type(objectPath) == "string", "D-Bus object path must be specified")
    self.busName = busName
    self.objectPath = objectPath
    self.signals = {}
    self.signalRawMode = {}
    self.proxy = nil
    self.bus = nil
end


--- Attaches the proxy to the specified D-Bus connection.
-- This method also creates a new proxy object.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @return nil (on failure) or an empty string on success
-- @return @{ldbus.ErrorInfo} which is a table: {errCode, errMsg}
function M.Proxy:bind(bus)
    local errInfo, result = verify(type(bus) == "table", "D-Bus bus must be provided")
    if errInfo then
        return nil, errInfo
    end
    
    if self:isBound() then
        result, errInfo = bus.destroyProxy(self.proxy)
        if not result then
            return nil, errInfo
        end
        self.proxy = nil
    end
    
    self.proxy, errInfo = bus.newProxy(self.busName, self.objectPath)
    if not self.proxy then
        return nil, errInfo
    end
    
    self.bus = bus
    self.proxy.subscribe(nil, function(s, p) return self:_onSignal(s, p) end)
    
    return true, makeErrInfo()   
end


--- Detaches the proxy from the D-Bus bus and service.
-- This method also destroys the underlying proxy object.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @return nil (on failure) or 'true' on success
-- @return @{ldbus.ErrorInfo} which is a table: {errCode, errMsg}
function M.Proxy:unbind()
    if not self:isBound() then
        return true, makeErrInfo()
    end
    
    self.proxy.unsubscribe(nil)
    local result, errInfo = self.bus.destroyProxy(self.proxy)
    if not result then
        return nil, errInfo
    end
    
    self.proxy = nil
    self.bus = nil
    
    return true, makeErrInfo()
end


--- Indicates whether or not the proxy was/is bound to the service.
-- This does <B>not</B> indicate whether the proxied service
-- currently exists on the bus at this moment. It is more an
-- indication that @{Proxy:bind} was called at one point. 
-- @treturn boolean Returns 'true' if the proxy was bound to
-- the service and 'false' otherwise.
function M.Proxy:isBound()
    return self.proxy ~= nil
end


--- Subscribes to a ServiceProvider signal.
--@tparam string sigName The name of the signal to subscribe to
--@tparam function func The function to call when the signal is received. The
-- protoype for this function is func(sigName, payload) where 'sigName' is 
-- the name of the signal and 'payload' is either a Lua type or string
-- depending if the signal is subscribed in 'rawMode'
-- @tparam boolean rawMode Set to 'true' if the signal payload should
-- <B>not</B> be JSON decoded and provided as a raw string. If false (or nil)
-- the signal will be decoded from JSON to a Lua type.
-- @return None
function M.Proxy:subscribe(sigName, func, rawMode)
    assert( (type(sigName) == "string") and (#sigName > 0), "Invaid signal name")
    assert( type(func) == "function" )
    
    self.signals[sigName] = func
    self.signalRawMode[sigName] = rawMode or nil
end


--- Unsubscribe from the specified ServiceProvider signal name.
-- @tparam string sigName The name of the signal to unsubscribe
-- @return None
function M.Proxy:unsubscribe(sigName)
    assert( (type(sigName) == "string") and (#sigName > 0), "Invaid signal name")
    self.signals[sigName] = nil
    self.signalRawMode[sigName] = nil
end


-- (Private) Handles all signals directed to this proxy.
-- This method dispatches a signal to appropriate handler and
-- optionally try to decode the payload as JSON.
function M.Proxy:_onSignal(sigName, payload)
    local f = self.signals[sigName]
    if f then
        -- If we should deliver this signal without decoding
        -- the JSON payload then
        if self.signalRawMode[sigName] then
            f(sigName, payload)
        else
            local result, msg = cjson.decode(payload)
            if result then
                f(sigName, result)
            else
                io.stderr:write("OnSignal <" .. sigName .. "> decode error: " .. msg)
            end
        end
    end
end


--- Makes a request to a ServiceProvider service.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @tparam string method The service method being requested
-- @param payload String or Lua type that will be interpreted by the
-- 'rawMode' option.
-- @tparam boolean ignoreReply If 'true' then send the request and don't
-- block waiting for the reply. If 'false' or nil the send the request and block
-- waiting for the reply to decode (if not in rawMode).
-- @tparam boolean rawMode If 'true' then no JSON encoding is applied to the payload of
-- a request. If option evalues to 'false' then the payload argument is assumed
-- to be a Lua type that should be converted to JSON. Likewise, the result will
-- be decoded from JSON to a Lua type of rawMode evaluates to false. Otherwise
-- no attempt is made to decode the result.
-- @return nil (on failure) or the result of the request. If ignoring the
-- reply then 'true' is returned on successful request submission, nil
-- otherwise.
-- @return @{ldbus.ErrorInfo} which is a table: {errCode, errMsg}
function M.Proxy:request(method, payload, ignoreReply, rawMode)
    local result, errInfo = nil
    ignoreReply = (ignoreReply == true) or false
    rawMode = (rawMode == true) or nil
    if rawMode then
        result, errInfo = self.proxy.request(method, payload, ignoreReply)
    else
        result, errInfo = self.proxy.request(method, cjson.encode(payload), ignoreReply)
    end
    if not result then
        return nil, errInfo
    else
        if ignoreReply then
            return true, nil
        elseif rawMode then
            return result, errInfo
        else
            result, errInfo = cjson.decode(result)
            if result then
                return result, makeErrInfo()
            else
                return nil, verify(false, errInfo)
            end
        end
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
