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
@file           service.lua
@author         Glenn Schmottlach
@brief          Service helper class that utilizes legacy ldbus module.
*****************************************************************************
--]]

--- XSe ServiceProvider Service Helper Class
-- This module wraps much of the service-oriented functionality of
-- the ldbus module and exposes a friendlier API packaged as
-- a inheritable class (Service). The intent is for service
-- developers to inherit from the Service class and extend
-- it with method handlers.
-- @module service
-- @alias M
-- @author Glenn Schmottlach
-- @copyright 2013 XS-Embedded LLC
-- @license MIT License (MIT)

local ldbus = require("ldbus")
local cjson	= require("cjson.safe")
local class	= require("pl.class")

local XSE_INTERFACE = "com.xsembedded.ServiceProvider"
local XSE_ERROR_NAME = "com.xsembedded.ServiceProvider.Error"
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


--- Registers a well-known D-Bus bus name on the specified bus.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @tparam table bus The D-Bus bus on which to register the well-known name
-- @tparam string busName The well-known D-Bus name to register
-- @return nil or an empty string on success
-- @return @{ldbus.ErrorInfo} which is a table: {errCode, errMsg}
function M.registerBusName(bus, busName)
	local errInfo = verify(type(bus) == "table", "Missing D-Bus bus parameter") or
                    verify(type(busName) == "string" and
	                       ldbus.isValidWellKnownBusName(busName), "Invalid D-Bus bus name")
	if errInfo then
	   return nil, errInfo
	end	                 
	
	return bus.requestName(ldbus.strip(busName), ldbus.DBUS_NAME_FLAG_DO_NOT_QUEUE)
end

--- A D-Bus com.xsembedded.ServiceProvider service class
-- @type Service
M.Service = class()

--- Contructor for the Service class.
-- @tparam string objectPath The D-Bus object path associated with this service
-- @tparam boolean skipMethodReg A flag set 'true' to skip automatic
-- service method registration
-- @tparam string methodPrefix The prefix used to identify methods implementing
-- service handlers in the class. By default this is "m_" although registration
-- only occurs if the previous parameter (skpMethodReg) is set to true
-- @return None
function M.Service:_init(objectPath, skipMethodReg, methodPrefix)
	-- Validate the input parameters
	assert((type(objectPath) == "string"), "Object path must be a string")
	assert(ldbus.isValidObjectPath(objectPath), "Invalid D-Bus object path")
	
	-- Initialize the instance variables
	self.bus = nil
	self.adaptor = nil
	self.methods = {}
	self.options = {}
	self.objectPath = ldbus.strip(objectPath)
	
	if not skipMethodReg then
	   self:enrollMethods(methodPrefix or "m_")
	end
end


--- Enrolls and registers class methods as D-Bus request handlers.
-- This method will scan through the methods defined for this class
-- and register those methods with a matching prefix as handlers for a
-- corresponding ServiceProvider method of the same name. So a method
-- like 'm_echo" would be registered as a handler for a ServiceProvider
-- 'echo' method.
-- @tparam string methodPrefix The prefix used to identify methods implementing
-- service handlers in the class. By default this is "m_" if 'nil' is passed
-- as an argument.
-- @return None
function M.Service:enrollMethods(methodPrefix)
	local prefix = methodPrefix or "m_"

	for k,v in pairs(getmetatable(self)) do
		local name = string.match(k, "^" .. prefix .. "(.+)")
		if name ~= nil then
			self:addMethod(name, function(ctx, arg) return v(self, ctx, arg) end)
		end
	end
end


--- Detaches the service from the underlying D-Bus connection.
-- This method also destroys the underling adaptor object.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @return nil (on failure) or an empty string on success
-- @return @{ldbus.ErrorInfo} which is a table: {errCode, errMsg}
function M.Service:detach()
	local errInfo, result = verify(type(self.bus) == "table" and
	                               self.adaptor ~= nil, "Object is not registered")
	-- Bail-out fast if there is an error with the input parameters
	if errInfo then
	   return nil, errInfo
	end
	
	result, errInfo = self.bus.destroyAdaptor(self.adaptor)
	if errInfo.errCode == ldbus.ERR_OK then
    	self.bus = nil
    	self.adaptor = nil
    end
    
    return result, errInfo
end


--- Attaches the service to the specified D-Bus connection.
-- This method also creates a new adaptor object.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @return nil (on failure) or 'true' on success
-- @return @{ldbus.ErrorInfo} which is a table: {errCode, errMsg}
function M.Service:attach(bus)
	local errInfo, result = verify(type(bus) == "table", "Invalid bus object") or 
	                        verify(self.adaptor == nil, "Service already registered")
	
	if errInfo then
	   return nil, errInfo
	end
	
	self.adaptor, errInfo = bus.newAdaptor(self.objectPath)
	
	if not self.adaptor then
	   return nil, errInfo
	end

	self.bus = bus
	-- Register a method that handles all requests to this service
	result, errInfo = self.adaptor.registerMethod(nil,
		function(context, method, payload)
			local handler = self.methods[method]
			local options = self.options[method]
			if handler == nil then
                -- Make a best effort delivery of the D-Bus error message
				self.adaptor.dbusError(context, XSE_ERROR_NAME, "Unknown method: " .. method)
			else
				local convertedArgs = payload
				local err = nil
				if options.rawMode ~= true then
					convertedArgs, err = cjson.decode(payload)
					if convertedArgs == nil then
                        self.adaptor.dbusError(context, XSE_ERROR_NAME,
                                                "Invalid JSON arguments: " .. err)
						return
					end
				end
				
				-- Call the handler to process the request. We can't do this with
				-- a pcall() since the handler may yield
				handler(context, convertedArgs)
			end
		end)
		
	-- Massage a returned value
	if result then
	   return result, makeErrInfo()
	else
	   return nil, verify(false, errInfo)
	end
end


--- Registers a named ServiceProvider method with a handler.
-- @tparam string name The ServiceProvider method name to register
-- @tparam function func The function that will handle service requests
-- @tparam boolean rawMode Set to 'true' if the argument to the method
-- should skip the JSON decode step and provided raw. If false (or nil)
-- the service request arguments will be decoded from JSON to Lua types.
-- @return None
function M.Service:addMethod(name, func, rawMode)
	local method = ldbus.strip(name)
	assert(type(method) == "string" and (#method > 0), "Invalid method name")
	assert(type(func) == "function", "Invalid callback function")

	self.methods[method] = func
	self.options[method] = {rawMode = (rawMode == true or false)}
end


--- Detaches the handler from the specified ServiceProvider method.
-- @tparam string name The name of the method to detach
-- @return None
function M.Service:removeMethod(name)
	assert(type(name) == "string")
	local method = ldbus.strip(name)
	assert(#method > 0, "Method name must be provided")
	self.methods[method] = nil
	self.options[method] = nil	
end


--- Sends a notification (D-Bus signal).
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @tparam string sigName The name of the ServiceProvider signal
-- @param payload A string or Lua table containing the Lua argument to emit
-- @tparam boolean rawMode If it evaluates to 'true' then the payload
-- is not JSON encoded and treated as a string. If 'false' then the payload
-- is considered to be a Lua table and is encoded to a JSON object before
-- being sent out.
-- @return nil (on failure) or an empty string on success
-- @return @{ldbus.ErrorInfo} which is a table: {errCode, errMsg}
function M.Service:notify(sigName, payload, rawMode)
	local errorInfo, result = verify(type(self.adaptor) == "table", "Service is not registered")
	if errorInfo then
	   return nil, errorInfo
	end
	
	local p = payload
	local err = nil
	if rawMode ~= true then
		p, err = cjson.encode(payload)
		if not p then
		  return nil, verify(false, "Failed to encode the payload: " .. err)
		end
	end
	
	return self.adaptor.notify(sigName, p)
end


--- Sends a D-Bus reply to a request.
-- For any given request, only a @{Service:reply} or @{Service:replyError} can be
-- called - but <B>not</B> both.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @tparam userdata context The context associated with the request
-- @param payload A string or Lua table containing the Lua argument to reply
-- @tparam boolean rawMode If it evaluates to 'true' then the payload
-- is not JSON encoded and treated as a string. If 'false' then the payload
-- is considered to be a Lua table and is encoded to a JSON object before
-- being sent out
-- @return nil (on failure) or an empty string on success
-- @return @{ldbus.ErrorInfo} which is a table: {errCode, errMsg}
function M.Service:reply(context, payload, rawMode)
    local errorInfo, result = verify(type(self.adaptor) == "table", "Service is not registered") or
                              verify(type(context) == "table", "Context is not valid")
    if errorInfo then
       return nil, errorInfo
    end

    local p = payload
    local err = nil
    if rawMode ~= true then
        p, err = cjson.encode(payload)
        if not p then
          return nil, verify(false, "Failed to encode the payload: " .. err)
        end
    end
    
    return self.adaptor.reply(context, p)
end


--- Sends a D-Bus error in response to a request.
-- For any given request, only a @{Service:reply} or @{Service:replyError} can be
-- called - but <B>not</B> both.
-- <li><B>NOTE:</B> Cannot be invoked within a protected call (pcall)
-- @tparam userdata context The context associated with the original request
-- @param payload A string or Lua table containing the Lua argument to reply
-- @tparam boolean rawMode If it evaluates to 'true' then the payload
-- is not JSON encoded and treated as a string. If 'false' then the payload
-- is considered to be a Lua table and is encoded to a JSON object before
-- being sent out
-- @return nil (on failure) or an empty string on success
-- @return @{ldbus.ErrorInfo} which is a table: {errCode, errMsg}
function M.Service:replyError(context, payload, rawMode)
    local errorInfo, result = verify(type(self.adaptor) == "table", "Service is not registered") or
                              verify(type(context) == "userdata", "Context is not valid")
    if errorInfo then
       return nil, errorInfo
    end

    local p = payload
    local err = nil
    if rawMode ~= true then
        p, err = cjson.encode(payload)
        if not p then
          return nil, verify(false, "Failed to encode the payload: " .. err)
        end
    end
    
    return self.adaptor.dbusError(context, ldbus.DEFAULT_ERROR_NAME, p)
end


-- Called when this module is run as a program
local function main(arg)
    print(string.match(arg[0], "^(.+)%.lua") .. " - Version: " .. VERSION)
end


-- Determine the context in which the module is usd
if require('is_main')() then
    -- The module is being run as a program
    main(arg)
else
    -- The module is being loaded rather than run
    return M
end
