--[[
*****************************************************************************
Project         l2dbus

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
@file           proxyctrl.lua
@author         Glenn Schmottlach
@brief          Provides a proxy interface D-Bus services.
*****************************************************************************
--]]

--- Proxy Controller Module.
-- This module provides an abstract controller/proxy client class library
-- for communicating with a remote D-Bus service. Based on either the D-Bus
-- XML introspection data for a service or an explicit description in Lua, the
-- class provides a mechanism to dynamically generate a true proxy interface
-- for the methods and properties exposed by the remote D-Bus service.
-- </br></br>
-- See the description of the @{getProxy} method for a better understanding
-- on how to use the proxied interfaces to call methods or get/set properties
-- of an interface.
-- 
-- @module l2dbus.proxyctrl
-- @alias M


local l2dbus = require("l2dbus")
local xml = require("l2dbus.xml")
local validate = require("l2dbus.validate")

local verifyTypes			=	validate.verifyTypes
local verifyTypesWithMsg	=	validate.verifyTypesWithMsg
local verify				=	validate.verify

local M = { }
local ProxyController = { __type = "l2dbus.lua.proxy_controller" }
ProxyController.__index = ProxyController

--
-- Forward method declarations
--
local newMethodProxy
local newPropertyProxy


--- Constructs a new ProxyController instance.
-- 
-- The constructor creates a ProxyController instance. As its name implies
-- the object controls the behavior and configuration of a proxy. These
-- configurable items include things like the timeout used by the proxy,
-- blocking mode, and the actual dynamic generation of the proxy itself based
-- on metadata gleaned from the remote object (via D-Bus introspection or
-- provided directly). The ProxyController exposes the actual remote service
-- proxy including methods and properties via separate objects that eliminate
-- *namespace* collisions between the controller's methods and those of the
-- remote service.
-- 
-- @tparam userdata conn The @{l2dbus.Connection|Connection} to attach
-- the controller to.
-- @tparam string busName The D-Bus bus name on which the remote service is
-- offered.
-- @tparam string objPath The remote service's object path.
-- @treturn table A proxy controller instance.
function M.new(conn, busName, objPath)
	verify(type(conn) == "userdata", "invalid connection")
	verify(validate.isValidBusName(busName), "invalid D-Bus bus name")
	verify(validate.isValidObjectPath(objPath), "invalid D-Bus object path")
	
	local proxyController = {
				conn = conn,
				busName = busName,
				objPath = objPath,
				signalHnds = {},
				introspectData = nil,
				blockingMode = false,
				timeout = l2dbus.Dbus.TIMEOUT_USE_DEFAULT,
				proxyCache = {},
				proxyNoReplyNeeded = false
				}
					
	return setmetatable(proxyController, ProxyController)
end


--- ProxyController
-- @type ProxyController

--- Binds the controller to the remote service using D-Bus introspection.
-- 
-- This method will attempt to introspect the remote service associated with
-- this ProxyController. This implies that the @{l2dbus.Connection|Connection}
-- associated with the controller must be connected and the remote service
-- must support introspection. The method may throw a Lua error if an
-- exceptional (unexpected) error occurs.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @treturn true|nil Returns **true** if the binding operation succeeds
-- or **nil** on failure.
-- @treturn ?string|nil Returns an error name or **nil** if a name is
-- unavailable and the binding operation fails.
-- @treturn ?string|nil Returns an error message or **nil** if a message is
-- unavailable and the binding operation fails.
-- @function bind
function ProxyController:bind()
	if not self.introspectData then
		verify(self.conn:isConnected(), "not connected to the D-Bus bus")
		local msg = l2dbus.Message.newMethodCall({destination=self.busName,
								path=self.objPath,
								interface=l2dbus.Dbus.INTERFACE_INTROSPECTABLE,
								method="Introspect"})
		local reply, errName, errMsg = self:sendMessage(msg)
		if not reply then
			return nil, errName, errMsg
		end
		
		local result
		if self:getBlockingMode() then
			result = reply:getArgs()
			if type(result) ~= "string" then
				errName = l2dbus.Dbus.ERROR_FAILED 
				errMsg = "Failed to get introspection data"
			end
		else
			reply:block()
			local msg = reply:stealReply()
			if msg:getType() == l2dbus.Dbus.MESSAGE_TYPE_ERROR then
				result = nil
				errName = msg:getErrorName()
				errMsg = msg:getArgs()
			else
				-- Assume it's a reply message
				result = msg:getArgs()
				if type(result) ~= "string" then
					errName = l2dbus.Dbus.ERROR_FAILED 
					errMsg = "Failed to get introspection data"
				end
			end
		end
		
		if type(result) == "string" then
			self.introspectData = self:parseXml(result)
		else
			return nil, errName, errMsg
		end
		
		-- Clear the proxy cache so it will be re-generated when
		-- the client requests a new proxy again.
		self.proxyCache = {}
	end
	return true
end


--- Binds the controller to the remote service **without** D-Bus introspection.
-- 
-- This method uses the provided introspection data (either formatted as
-- D-Bus introspection XML or a Lua introspection table) to *bind* with the
-- remote service. This method makes **no** attempt to contact the remote
-- service for this data by sending messages on the bus. If the data is
-- provided as D-Bus introspection XML then it will internally be parsed
-- and converted into a Lua introspection table. The structure of a Lua
-- introspection table takes the following form:
-- 
-- 		{
-- 			["interface_name_1"] = {
-- 			
-- 				interface = "interface_name_1",
-- 					
-- 				properties = {
-- 					prop_name_1 = {
-- 						sig = "i",
-- 						access = "r"
-- 					},
-- 					prop_name_2 = {
-- 						sig = "s",
-- 						access = "rw"
-- 					},
-- 					...
-- 					prop_name_N = {
-- 						sig = "u",
-- 						access = "w"
-- 					},
-- 				},
-- 					
-- 				signals = {
-- 					sig_name_1 = {
-- 						{
-- 							sig = "s",
-- 							dir = "out"
-- 						}
-- 					}, 						
-- 					sig_name_2 = {
-- 						{
-- 							sig = "i",
-- 							dir = "out"
-- 						},
-- 						{
-- 							sig = "as",
-- 							dir = "out"
-- 						}
-- 					},
-- 					...
-- 					sig_name_N = {
-- 						{
-- 							sig = "t",
-- 							dir = "out"
-- 						},
-- 						{
-- 							sig = "i",
-- 							dir = "out"
-- 						}
-- 					}
-- 				},
-- 					
-- 				methods = {
-- 					method_name_1 = {
-- 						{
-- 							sig = "as",
-- 							dir = "out"
-- 						}
-- 					},
-- 					
-- 					method_name_2 = {
-- 						{
-- 							sig = "s",
-- 							dir = "in"
-- 						},
-- 						{
-- 							sig = "i",
-- 							dir = "out"
-- 						}
-- 					},
-- 					...
-- 					method_name_N = {
-- 						{
-- 							sig = "u",
-- 							dir = "in"
-- 						}
-- 					}
-- 				},
-- 			},
-- 			...
-- 			["interface_name_N"] = {
-- 				...
-- 			}
-- 
-- Introspection data that is **not** formatted as D-Bus XML introspection
-- data must adhere to the structure of the Lua introspection table above. This
-- table is comprised of one or more interface tables. Each interface table
-- has a properties, signals, and methods table. It also has a (seemingly)
-- redundant entry for the interface name which helps speed up interface
-- lookups. The method, signal, and property tables themselves can have
-- multiple entries with individual methods and signals having zero or more
-- arguments. When fed an XML formatted D-Bus interface description a similar
-- table is generated and stored internally.
-- 
-- **WARNING:** Validation is **not** done on a Lua introspectionData table
-- passed into this function. It is assumed to be structured correctly.
-- Minimal validation is done if this introspection data is passed in as XML.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam string|table introspectionData The introspection data either
-- expressed as the D-Bus XML introspection string or a Lua introspection
-- table as above. 
-- @treturn true Returns **true** if the binding operation succeeds. If the
-- introspection data cannot be parsed then a Lua error may be thrown.
-- @function bindNoIntrospect
function ProxyController:bindNoIntrospect(introspectData)
	verifyTypesWithMsg("string|table", "unexpected type for arg #2",
						introspectData)
		
	if type(introspectData) == "string" then
		self.introspectData = self:parseXml(introspectData)
	elseif type(introspectData) == "table" then
		self.introspectData = introspectData
	else
		error("D-Bus XML string or Lua introspection table expected")
	end
	-- Clear the proxy reference so it will be re-generated the next
	-- time the client asks for it
	self.proxyCache = {}
		
	return true
end


--- Unbinds the controller from the remote service.
-- 
-- This method unbinds or disconnects the ProxyController from the
-- remote service by effectively erasing any previous introspection
-- data. The call the @{bind} or @{bindNoIntrospect} must be executed again
-- in order to interact with the remote D-Bus service.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @function unbind
function ProxyController:unbind()
	self.introspectionData = nil
	self.proxyCache = {}
end


--- Retrieves the introspection data from the ProxyController.
-- 
-- This method returns the D-Bus introspection data as a Lua table
-- described in the documentation for @{bindNoIntrospect}. This can be
-- useful to understand how D-Bus XML introspection data is converted to
-- a Lua table representation.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @function getIntrospectionData
function ProxyController:getIntrospectionData()
	return self.introspectData
end


--- Retrieves the actual proxy for the remote D-Bus service.
-- 
-- This method returns a proxy for the named interface. The actual
-- proxy has two sub-objects named *p* and *m*. These sub-objects split
-- the interface namespace into properties (*p*) and methods (*m*) avoiding
-- the possibility of name collisions. Before you can get the proxy the
-- ProxyController must be @{bind|bound} to a remote service. An example
-- of how to access methods or properties on a remote service is shown below:
--
--		-- Example of making a BLOCKING call on a proxy 
-- 		local proxyCtrl, proxy, status, pending, reply, enabled
-- 		proxyCtrl = proxy.new(conn, "org.freedesktop.NetworkManager",
--							/org/freedesktop/NetworkManager")
--		-- Bind to the remote service
-- 		proxyCtrl:bind()
-- 		-- Put the controller in blocking mode
-- 		proxyCtrl:setBlockingMode(true)
-- 		-- Get the actual proxy for the interface we're interested in
-- 		proxy = proxyCtrl:getProxy("org.freedesktop.NetworkManager")
-- 		status, names = proxy.m.GetDevices()
-- 		if status then
-- 			print("We got device names")
-- 		end
-- 		-- Use the "getter" sub-object to read the value of a property
-- 		status, enabled = proxy.p.get.WirelessEnabled()
-- 		if status then
-- 			-- Set the property but indicate we're not interested
-- 			-- in the response ('true' == no response needed)
--			proxy.p.set.WirelessEnabled(not enabled, true)
--		end
-- 		
--
--		-- Example of making a NON-BLOCKING call on a proxy
-- 		local proxyCtrl, proxy, status, pending, reply, enabled 
-- 		proxyCtrl = proxy.new(conn, "org.freedesktop.NetworkManager",
--							"/org/freedesktop/NetworkManager")
-- 		proxyCtrl:bind()
-- 		-- The proxy calls to the remote service are non-blocking now
-- 		proxyCtrl:setBlockingMode(false)
-- 		-- Get the proxy for the interface we're interested in
-- 		proxy = proxyCtrl:getProxy("org.freedesktop.NetworkManager")
-- 		-- If everything goes smoothly (status == true) the 'pending' is
-- 		-- a PendingCall object.
-- 		status, pending = proxy.m.GetDevices()
-- 		if status then
-- 			-- Wait to get a reply. Will yield if called from a coroutine
-- 			-- other than the "main" one.
-- 			status, reply = proxyCtrl:waitForReply(pending)
-- 			if status then
-- 				print("We got device names")
-- 			end
-- 		end
-- 		-- Properties are retrieved in the same way as before
-- 		status, pending = proxy.p.get.WirelessEnabled()
-- 		if status then
-- 			-- Wait for the property "get" request to complete
-- 			status, enabled = proxyCtrl:waitForReply(pending)
-- 			if status then
-- 				print("We got the Wireless state")
-- 				-- Again, set the property but indicate (true) that no
-- 				-- reply is needed.
-- 				proxy.p.set.WirelessEnabled(enabled, true)
-- 			end
-- 		end
--
-- The method and property sub-objects expose the remote methods and properties
-- of a bound service interface. The *property* sub-object (p) is further
-- split into properties that are writable (e.g. can be *set*) and those that
-- can only be read (e.g. *get*). So the *p* sub-object of a proxy has two
-- sub-objects called *set* and *get*. Beneath these sub-objects are the names
-- of all the properties for that interface split across these two set/get
-- boundaries. For properties that are read/write the identical name may appear
-- under both *set* and *get*.
-- </br></br>
-- Methods or properties that are called will typically return two (or more)
-- parameters at a minimum.
-- <ul>
-- <li>**status** (bool) **True** if the call completed without error, **false**
-- otherwise.</li>
-- </p>
-- <li>**arg1..argN|PendingCall|errName** (any|userdata|string) If **status**
-- is **true** and proxy calls are configured to expect a reply, if the
-- ProxyController is configured to be blocking, the remote service return
-- values (arg1..argN) are returned. A non-blocking call expecting a reply will
-- result in a @{l2dbus.PendingCall|PendingCall} being returned. If no reply
-- is expected then this argument will be the D-Bus serial number of the
-- request message.
-- If the **status** is **false** then generally a D-Bus error name is
-- returned.</li>
-- </p>
-- <li>**errMsg** (string|nil) If **status** were **false** then the third
-- parameter would be an optional error message or **nil** if one
-- is not available.</li>
-- </ul>
-- </br></br>
-- Method and property calls can also result in a Lua error being thrown.
-- Generally these are reserved for truly exceptional conditions or programming
-- errors (e.g. wrong parameters, types, etc...). D-Bus error messages returned
-- by a remote service **do not** generate Lua errors but rather the error
-- name and message are returned with **status** set to **false**. In general
-- the calls on the proxy do not need to be made with a Lua protected
-- call (*pcall*) unless every possible exception needs to be caught.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam string interface The D-Bus interface name for which to retrieve
-- the proxy. This interface **must** be an element of the introspection
-- data. A Lua error is generated if the interface is not supported.
-- @treturn table The actual proxy for the remote service.
-- @function getProxy
function ProxyController:getProxy(interface)
	verify(validate.isValidInterface(interface), "invalid D-Bus interface")
	verify( self.introspectData, "controller is not bound to service object")

	if self.proxyCache[interface] == nil then
		local metadata = self.introspectData[interface]
		if not metadata then
			error("service object does not implement " .. interface)
		end
		
		self.proxyCache[interface] = { m = newMethodProxy(self, metadata),
										p = newPropertyProxy(self, metadata) }
	end
	
	return self.proxyCache[interface]	
end


--- Sets the timeout to use for all proxy requests.
-- 
-- This method sets the timeout used by all subsequent proxied calls on the
-- remote service. The timeout is specified in milliseconds. It is not possible
-- to set the timeout individually for each proxy method/property call unless
-- this method is called prior to the method/property invocation.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam number timeout The timeout (in milliseconds) to use for subsequent
-- proxy requests. Two special values are allowed as well:
-- @{l2dbus.Dbus.TIMEOUT_USE_DEFAULT|TIMEOUT_USE_DEFAULT}
-- and @{l2dbus.Dbus.TIMEOUT_INFINITE|TIMEOUT_INFINITE}. The default
-- value (if none is specified) is @{l2dbus.Dbus.TIMEOUT_USE_DEFAULT|TIMEOUT_USE_DEFAULT}.
-- @function setTimeout
function ProxyController:setTimeout(timeout)
	verify("number" == type(timeout), "timeout must be a number")
	self.timeout = timeout
end


--- Gets the timeout used for all proxy requests.
-- 
-- This method gets the timeout used by all proxied calls on the remote
-- service. The timeout is specified in milliseconds but may have two
-- special values: @{l2dbus.Dbus.TIMEOUT_USE_DEFAULT|TIMEOUT_USE_DEFAULT}
-- and @{l2dbus.Dbus.TIMEOUT_INFINITE|TIMEOUT_INFINITE}.
--
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @treturn number The proxy-wide timeout value in milliseconds.
-- @function getTimeout
function ProxyController:getTimeout()
	return self.timeout
end


--- Sets the blocking mode used by the ProxyController to make calls.
-- 
-- This method sets the blocking mode (**true** == blocking, **false** ==
-- non-blocking) used by proxied calls to a remote service. Blocking calls
-- will in fact block the thread of Lua execution. Non-blocking calls will
-- return a @{l2dbus.PendingCall|PendingCall} object which the caller can use
-- to either block waiting for an answer or be notified later that a reply
-- has arrived.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam bool mode Set to **true** for blocking mode,
-- **false** for non-blocking mode.
-- @function setBlockingMode
function ProxyController:setBlockingMode(mode)
	self.blockingMode = mode and true or false
end


--- Gets the blocking mode used by the ProxyController to make calls.
-- 
-- This method returns the blocking mode of the ProxyController. If it returns
-- **true** then it will make *blocking* proxy calls. If it returns **false**
-- then it's configured to make *non-blocking* calls.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @treturn bool The blocking mode where **true** == blocking,
-- **false** == non-blocking.
-- @function getBlockingMode
function ProxyController:getBlockingMode()
	return self.blockingMode
end


--- Sets whether proxy method calls expect/need a reply from the far-end.
-- 
-- This method determines whether proxy calls to the far-end need a response.
-- Specifically, if set to **true**, out-going messages are marked to indicate
-- that a reply is not needed so that the far-end can decide whether or not to
-- reply to the message. The far-end can always reply regardless of the flag
-- but it means the near-end will ignore the reply. The default setting for
-- the controller is **false** which means replies are expected from the
-- far-end. If set to **true** out-going messages are marked to indicate a
-- reply is not needed by using the call to @{sendMessageNoReply} to transmit
-- the request message rather than @{sendMessage}. This setting applies to
-- all subsequent proxy *method* calls made *after* the value is changed. The
-- behavior of individual proxy method calls can only be controlled by calling
-- this method *before* making the method call on the proxy. 
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam bool mode Set to **true** if no reply is needed/expected,
-- **false** to indicate a reply is expected.
-- @function setProxyNoReplyNeeded
function ProxyController:setProxyNoReplyNeeded(noReply)
	self.proxyNoReplyNeeded = noReply and true or false
end


--- Indicates whether proxy calls need/expect a reply from the far-end.
-- 
-- This method returns an indication of whether proxy method calls expect a
-- reply from the far-end. If this returns **true** then no reply is needed
-- and proxy method calls will **not** wait to hear the reply to a request.
-- If **false** is returned (the default behavior) then it is expected that
-- proxy method calls will return a reply message.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @treturn bool Returns **true** if no reply is expected from the far-end,
-- **false** (the default) if every proxy request expects a reply.
-- @function getProxyNoReplyNeeded
function ProxyController:getProxyNoReplyNeeded()
	return self.proxyNoReplyNeeded
end

--- Connects a handler to an interface's signal.
-- 
-- This method registers a D-Bus *Match* handler for a signal on a specific
-- interface.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam string interface A valid D-Bus interface name. This interface
-- must be defined in the introspection data for this ProxyController.
-- @tparam string sigName The name of the D-Bus signal to connect the handler.
-- @tparam func handler The handler that is called when the signal is received.
-- The signature of this handler is as follows:
-- 		function onSignal(arg1, arg2, ..., argN)
-- 			...
-- 		end
-- Where the arguments (argN) are defined the same as specified in the D-Bus
-- introspection XML description for the signal.
-- @treturn lightuserdata An opaque handle to the connection that can be used
-- later to disconnect the handler.
-- @function connectSignal
function ProxyController:connectSignal(interface, sigName, handler)
	verify(validate.isValidInterface(interface), "invalid D-Bus interface")
	verify(validate.isValidMember(sigName), "invalid D-Bus signal name")
	verify("function" == type(handler))
	
	local signalFilter = {msgType=l2dbus.Dbus.MESSAGE_TYPE_SIGNAL,
							interface=interface,
							path=self.objPath,
							member=sigName}

	local function onSignal(match, msg, handleSig)
		handleSig(msg:getArgs())
	end
	
	local hnd = self.conn:registerMatch(signalFilter, onSignal, handler)
	if hnd then
		self.signalHnds[hnd] = true
	end
	
	return hnd
end


--- Disconnects the specified handler from the D-Bus signal.
-- 
-- Given the opaque handle returned by @{connectSignal} this method can
-- be used to disconnect the handler from that signal.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam lightuserdata hnd The opaque handle returned by @{connectSignal}
-- @treturn bool Returns **true** if disconnected successfully, **false**
-- otherwise.
-- @function disconnectSignal
function ProxyController:disconnectSignal(hnd)
	local disconnected = self.conn:unregisterMatch(hnd)
	if disconnected then
		self.signalHnds[hnd] = nil
	end
	
	return disconnected
end


--- Disconnects all the signal handlers from the ProxyController.
-- 
-- The method disconnects **all** the signal handlers that have been connected
-- to all the interfaces. If a signal fails to be disconnected a Lua error
-- may be thrown indicating the disconnect failed.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @function disconnectAllSignals
function ProxyController:disconnectAllSignals()
	for hnd,_ in pairs(self.signalHnds) do
		if not self.conn:unregisterMatch(hnd) then
			error("failed to disconnect signal (hnd=" .. tostring(hnd) .. ")")
		end
	end
	
	self.signalHnds = {}
end


--- Sends a D-Bus message depending on the blocking mode of the ProxyController.
-- 
-- This method sends a D-Bus message depending on the blocking mode of the
-- ProxyController. If the blocking mode is set to *blocking* then the call
-- will wait on a reply (or timeout) and if everything is successful a
-- D-Bus message of type @{l2dbus.Message.METHOD_RETURN|METHOD_RETURN} is
-- returned. *Non-blocking* calls (on success) will return a
-- @{l2dbus.PendingCall|PendingCsendMessageNoReplyall} object that the caller can use to
-- @{waitForReply|wait} on or be notified of the reply. Proxy calls,
-- internally use this method to execute calls to remote services.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam userdata msg The D-Bus message to send.
-- @treturn userdata|nil If the controller is configuring in a *non-blocking*
-- mode then return a @{l2dbus.PendingCall|PendingCall} object if the message
-- is sent successfully otherwise return **nil**. If the controller is
-- configured in a *blocking* mode then either return the reply D-Bus message
-- (type = @{l2dbus.Message.METHOD_RETURN|METHOD_RETURN}) or **nil** if a
-- D-Bus error message is returned or another error is detected.
-- @treturn string|nil If the first return argument is **nil** then return
-- the D-Bus error name associated with the error. If there was no error
-- or an error name was not provided then return **nil**.
-- @treturn string|nil If the first return argument is **nil** then return
-- an optional error message associated with the error. If there was no error
-- or a message was not provided then return **nil**.
-- @function sendMessage
function ProxyController:sendMessage(msg)
	verifyTypesWithMsg("userdata", "unexpected type for arg #1", msg)
	
	local reply = nil
	local errName = nil
	local errMsg = nil
	
	-- If we're making a blocking call (no matter which coroutine
	-- thread) then ...
	if self.blockingMode then
		-- We completely block the Lua VM making this call
		reply, errName, errMsg = self.conn:sendWithReplyAndBlock(msg, self.timeout)
	-- Else this is a non-blocking call
	else
		local status, pending = self.conn:sendWithReply(msg, self.timeout)
		if not status then
			reply, errName, errMsg = nil, l2dbus.Dbus.ERROR_FAILED,
									"failed to send message"
		else
			reply, errName, errMsg = pending, nil, nil
		end
	end
	
	return reply, errName, errMsg
end


--- Sends a D-Bus message indicating it does not expect a reply.
-- 
-- This method provides an optimization for the called service indicating
-- the client is not waiting for a reply. The called service may either
-- choose not to send a reply, or if it does, the reply will be ignored and
-- discarded by the client. It's intent is to reduce the amount of round-trip
-- messaging to a minimum.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam userdata msg The D-Bus message to send.
-- @treturn bool Returns **true** if the message is sent successfully or
-- **false** if there was an error.
-- @treturn number|nil Returns the D-Bus serial number of the message if
-- sent successfully or **nil** if it could not be sent.
-- @function sendMessageNoReply
function ProxyController:sendMessageNoReply(msg)
	verifyTypesWithMsg("userdata", "unexpected type for arg #1", msg)
	msg:setNoReply(true)	
	return self.conn:send(msg)
end


--- Waits for a reply from a pending call.
-- 
-- This method is called with a @{l2dbus.PendingCall|PendingCall} object
-- and uses it to wait for a reply from a remote service. How it
-- waits is largely dependent on whether or not this method was called
-- from the "main" Lua coroutine or a different one. Since the "main" Lua
-- coroutine cannot yield this call will translate into a purely blocking
-- call that will block the Lua VM. If it is **not** the main Lua
-- coroutine (or thread) then the coroutine will yield waiting for a
-- reply. When the reply or a timeout occurs the thread will be resumed
-- and the reply message returned. If this method is called from a secondary
-- coroutine then it **MUST NOT** be called via a Lua *pcall* (or 
-- protected call) since this *waitForReply* may yield and under Lua 5.1
-- yielding across a protected call is not allowed.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam userdata pendingCall The @{l2dbus.PendingCall|PendingCall} object.
-- @treturn userdata|nil The reply message of type
-- @{l2dbus.Message.METHOD_RETURN|METHOD_RETURN} or **nil** if a D-Bus
-- error message was returned or another error detected.
-- @treturn string|nil If the first return argument is **nil** then return
-- the D-Bus error name associated with the error. If there was no error
-- or an error name was not provided then return **nil**.
-- @treturn string|nil If the first return argument is **nil** then return
-- an optional error message associated with the error. If there was no error
-- or a message was not provided then return **nil**.
-- @function waitForReply
function ProxyController:waitForReply(pendingCall)
	verify("userdata" == type(pendingCall))
	
	local function onNotifyReply(pending, co)
		coroutine.resume(co, pending:stealReply())
	end
	
	local reply = nil
	local errName = nil
	local errMsg = nil
	
	if pendingCall:isCompleted() then
		reply = pendingCall:stealReply()
	else
		-- See if we're calling from the main thread
		local co = coroutine.running()
		if not co then
			-- The main thread cannot yield so we must explicity block
			pendingCall:block()
			reply = pendingCall:stealReply()
		else
			pendingCall:setNotify(onNotifyReply, co)
			reply = coroutine.yield()
		end
		assert( pendingCall:isCompleted() )
	end
	
	if not reply then
		reply, errName, errMsg = nil, l2dbus.Dbus.ERROR_FAILED, "no reply received"
	elseif l2dbus.Message.ERROR == reply:getType() then
		reply, errName, errMsg = nil, reply:getErrorName(), reply:getArgs()
	end
	
	return reply, errName, errMsg 
end


--- Parses D-Bus introspection XML data a returns a Lua table equivalent.
-- 
-- This method parses D-Bus introspection data and converts it to an internal
-- Lua table representation which is used to generate the necessary proxy
-- objects. The XML parser used to parse this data is **not** a full-blown,
-- fully validating parser and has known limitations. See the @{l2dbus.xml}
-- module for more details.
-- 
-- @within ProxyController
-- @tparam table ctrl The ProxyController instance.
-- @tparam string xmlStr A string containing valid D-Bus instrospection XML
-- describing the interfaces exposed by a service object.
-- @treturn table Returns a Lua table representation of the parsed XML
-- introspection. See @{bindNoIntrospect} for a description of the layout
-- of this table.
-- @function parseXml
function ProxyController:parseXml(xmlStr)
	verifyTypes("string", xmlStr)	
	local doc = xml.parse(xmlStr)
	local interfaces = {}
	for interface in doc:childtags() do
	    local methods = {}
	    local signals = {}
	    local properties = {}
	    for item in interface:childtags() do
	        if (item.tag == "method") or (item.tag == "signal") then
	            local args = {}
	            for arg in item:childtags() do
	                if item.tag == "method" then
	                    table.insert(args, {name = arg.attr.name,
	                    					sig=arg.attr.type,
	                    					dir=arg.attr.direction})
	                else
	                    table.insert(args, {name = arg.attr.name,
	                    					sig=arg.attr.type,
	                    					dir="out"})
	                end
	            end
	            if item.tag == "method" then
					methods[item.attr.name] = args
	            else
					signals[item.attr.name] = args
	            end
	        elseif item.tag == "property" then
	            local access = nil
	            if "read" == item.attr.access then
	                access = "r"
	            elseif "write" == item.attr.access then
	                access = "w"
	            else
	                access = "rw"
	            end
				properties[item.attr.name] = {sig=item.attr.type, access=access}
	        end
	    end
		interfaces[interface.attr.name] = { interface = interface.attr.name,
										   methods = methods,
			                               signals = signals,
			                               properties = properties}
	end
	
    return interfaces	
end


--
-- Constructor for the method (m) proxy.
--
newMethodProxy = function (proxyCtrl, metadata)
	verifyTypesWithMsg("table", "unexpected type for arg #1", proxyCtrl)
	verifyTypesWithMsg("table", "unexpected type for arg #2", metadata)
	
	local methodProxy = {}

	local function methodFunc(ctrl, metadata, method, methodInfo)		
		local innerFunc = function(...)
			local msg = l2dbus.Message.newMethodCall({destination=ctrl.busName,
							path=ctrl.objPath,
							interface=metadata.interface,
							method=method})
			if not msg then
				error("unable to create D-Bus method call message")
			end
			
			-- Pack in the arguments
			local callingArgs = {...}
			local inArgs = {}
			for idx = 1,#methodInfo do
				if methodInfo[idx].dir == "in" then
					table.insert(inArgs, methodInfo[idx])
				end
			end
			if #callingArgs ~= #inArgs then
				error(string.format("method argument mis-match for %s: provided=%d needed=%d",
						method, #callingArgs, #inArgs))
			else
				for idx=1,#inArgs do
					msg:addArgsBySignature(inArgs[idx].sig, callingArgs[idx])
				end
			end
			
			-- Determine if the proxy method call needs to wait around
			-- for a response.
			if ctrl:getProxyNoReplyNeeded() then
				local status, serNum = ctrl:sendMessageNoReply(msg)
				-- Let go of the reference since D-Bus now owns it
				msg:dispose()
				if status then
					return true, serNum
				else
					return false, l2dbus.Dbus.ERROR_FAILED,
							string.format("Unable to call method %s", method)
				end
			else
				local reply, errName, errMsg = ctrl:sendMessage(msg)
				-- Let go of the reference since D-Bus now owns it
				msg:dispose()
				if not reply then
					-- We failed to send the message or received an error
					-- in response
					return false, errName, errMsg
				elseif "l2dbus.message" == reply.__type then
					local replyArgs = {reply:getArgs()}
					-- Let go of the reference since the D-Bus message is no
					-- longer needed
					reply:dispose()
					return true, unpack(replyArgs)
				-- Else return the pending call
				else
					return true, reply	-- PendingCall object
				end
			end
		end
		
		return innerFunc
	end
	
	-- Create the methods for the proxy on-the-fly	
	for method, methodInfo in pairs(metadata.methods) do
		methodProxy[method] = methodFunc(proxyCtrl, metadata, method, methodInfo)
	end
		
	return methodProxy 
end

--
-- Constructor for the property (p) proxy.
--
newPropertyProxy = function(proxyCtrl, metadata)
	verifyTypesWithMsg("table", "unexpected type for arg #1", proxyCtrl)
	verifyTypesWithMsg("table", "unexpected type for arg #2", metadata)

	local propProxy = {set={}, get={}}	
	
	local function propGetFunc(ctrl, metadata, propName, propInfo)		
		local innerGetFunc = function()
			if not propInfo.access:find("r") then
				error(string.format("property '%s' is write-only", propName))
			end
			
			if not ctrl.introspectData[l2dbus.Dbus.INTERFACE_PROPERTIES] then
				error(string.format("object '%s' does not support %s",
						ctrl.objPath, l2dbus.Dbus.INTERFACE_PROPERTIES))
			end
			
			local msg = l2dbus.Message.newMethodCall({destination=ctrl.busName,
							path=ctrl.objPath,
							interface=l2dbus.Dbus.INTERFACE_PROPERTIES,
							method="Get"})
			if not msg then
				error("unable to create D-Bus method call message")
			end
			
			msg:addArgsBySignature("s", metadata.interface)
			msg:addArgsBySignature("s", propName)
			local reply, errName, errMsg = ctrl:sendMessage(msg)
			-- Let go of the reference since D-Bus now owns it
			msg:dispose()
			if not reply then
				return false, errName, errMsg
			elseif "l2dbus.message" == reply.__type then
				local replyArgs = {reply:getArgs()}
				-- Let go of the reference since the D-Bus message is no
				-- longer needed
				reply:dispose()
				return true, unpack(replyArgs)
			-- Else return the pending call
			else
				return true, reply	-- PendingCall object
			end
		end
		
		return innerGetFunc
	end
	
	local function propSetFunc(ctrl, metadata, propName, propInfo)
			
		local innerSetFunc = function(value, noReplyNeeded)
			local propInfo = metadata.properties[propName]
			if not propInfo then
				error("unknown property: " .. tostring(propName))
			end
			
			if not propInfo.access:find("w") then
				error(string.format("property '%s' is read-only", propName))
			end
			
			if not ctrl.introspectData[l2dbus.Dbus.INTERFACE_PROPERTIES] then
				error(string.format("object '%s' does not support %s",
						ctrl.objPath, l2dbus.Dbus.INTERFACE_PROPERTIES))
			end
			
			local msg = l2dbus.Message.newMethodCall({destination=ctrl.busName,
							path=ctrl.objPath,
							interface=l2dbus.Dbus.INTERFACE_PROPERTIES,
							method="Set"})
			if not msg then
				error("unable to create D-Bus method call message")
			end
			
			msg:addArgsBySignature("s", metadata.interface)
			msg:addArgsBySignature("s", propName)
			msg:addArgs(l2dbus.DbusTypes.Variant.new(value))
			local reply = nil
			local errName = nil
			local errMsg = nil
			if noReplyNeeded then
				reply = {ctrl:sendMessageNoReply(msg)}
				-- Dispose of the message since it's no longer needed
				msg:dispose()
				return unpack(reply)
			else
				reply, errName, errMsg = ctrl:sendMessage(msg)
				-- Dispose of the message since it's no longer needed
				msg:dispose()
				if not reply then
					return false, errName, errMsg
				elseif "l2dbus.message" == reply.__type then
					local replyArgs = {reply:getArgs()}
					-- Dispose of the reply since it's no longer needed
					reply:dispose()
					return true, unpack(replyArgs)
				-- Else return the pending call
				else
					return true, reply	-- PendingCall object
				end
			end
		end
		
		return innerSetFunc
	end
	
	-- Create the methods for the property proxy on-the-fly	
	for propName, propInfo in pairs(metadata.properties) do
		if propInfo.access:find("w") then
			propProxy.set[propName] = propSetFunc(proxyCtrl, metadata, propName, propInfo)
		end
		
		if propInfo.access:find("r") then
			propProxy.get[propName] = propGetFunc(proxyCtrl, metadata, propName, propInfo)
		end
	end
	
	return propProxy
end


-- Called when this module is run as a program
local function main(arg)
    print("Module: " .. string.match(arg[0], "^(.+)%.lua"))
    local info = l2dbus.getVersion()
    print("L2DBUS Version: " .. info.l2dbusVerStr)
    print("CDBUS Version: " .. info.cdbusVerStr)
    print(string.format("D-Bus Version: %d.%d.%d",
    		info.dbusMajor, info.dbusMinor, info.dbusRelease))
    print("Author: " .. info.author)
    print(info.copyright)
end

-- Determine the context in which the module is used
if l2dbus.isMain() then
    -- The module is being run as a program
    main(arg)
else
    -- The module is being loaded rather than run
    return M
end
