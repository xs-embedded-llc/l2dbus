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
@file           service.lua
@author         Glenn Schmottlach
@brief          Provides an object service framework for D-Bus services.
*****************************************************************************
--]]

--- Service Module.
-- This module provides a higher-level abstraction over the low level
-- @{l2dbus.ServiceObject|ServiceObject} class. This abstraction makes it
-- easier to implement a D-Bus service in Lua and is consequently the preferred
-- method for developing D-Bus services in Lua. 
-- 
-- @module l2dbus.service
-- @alias M

local l2dbus = require("l2dbus")
local xml = require("l2dbus.xml")
local validate = require("l2dbus.validate")

local verifyTypes			=	validate.verifyTypes
local verifyTypesWithMsg	=	validate.verifyTypesWithMsg
local verify				=	validate.verify

local M = { }
local Service = { __type = "l2dbus.lua.service" }
Service.__index = Service

local L2DBUS_ERROR_PROCESSING_REQUEST = "org.l2dbus.error.ProcessingRequest"
local DBUS_PROPERTIES_INTERFACE_NAME = "org.freedesktop.DBus.Properties"
local DBUS_PROPERTIES_INTERFACE_METADATA =
{
	properties = {
	},
	signals = {
		{
			name = "PropertiesChanged",
			args = {
				{
					sig = "s",
					name = "interface",
				},
				{
					sig = "a{sv}",
					name = "changedProps",
				},
				{
					sig = "as",
					name = "invalidatedProps",
				}
			}
		}
	},
	methods = {
		{
			name = "Get",
			args = {
				{
					sig = "s",
					name = "interface",
					dir = "in"
				},
				{
					sig = "s",
					name = "propname",
					dir = "in"
				},
				{
					sig = "v",
					name = "value",
					dir = "out"
				}
			}
		},
		{
			name = "Set",
			args = {
				{
					sig = "s",
					name = "interface",
					dir = "in"
				},
				{
					sig = "s",
					name = "propname",
					dir = "in"
				},
				{
					sig = "v",
					name = "value",
					dir = "in"
				}
			}
		},
		{
			name = "GetAll",
			args = {
				{
					sig = "s",
					name = "interface",
					dir = "in"
				},
				{
					sig = "a{sv}",
					name = "props",
					dir = "out"
				}
			}
		}
	},
	interface = DBUS_PROPERTIES_INTERFACE_NAME
}

--
-- Forward method declarations
--
local newReplyContext


--
-- Calculates the D-Bus signature of a method or signal.
--
local function calcSignatureFromMetadata(member, dir, metadata)
	local nMethods = #metadata.methods
	local signature = ""
	local argDir = nil
	for memIdx = 1, nMethods do
		if metadata.methods[memIdx].name == member then
			local nArgs = #metadata.methods[memIdx].args
			for argIdx = 1, nArgs do
				-- If the direction isn't specified then it's assumed to
				-- be an input parameter
				argDir = metadata.methods[memIdx].args[argIdx].dir or "in"
				if argDir == dir then
					signature = signature .. metadata.methods[memIdx].args[argIdx].sig
				end
			end
			break 
		end
	end
	return signature
end


--
-- Dispatches D-Bus requests to the appropriate handler.
--
local function globalHandler(lowLevelObj, conn, msg, svcObj)	
	local intfName = msg:getInterface()
	local member = msg:getMember()
	local handler = nil
	local dbusResult = l2dbus.Dbus.HANDLER_RESULT_HANDLED
	local context = nil
	local status = nil
	local result = nil
	local outSig = nil
	
	-- Search for a suitable handler
	if intfName and svcObj.interfaces[intfName] then
		if svcObj.interfaces[intfName].methods[member] then
			handler = svcObj.interfaces[intfName].methods[member].handler
			outSig = svcObj.interfaces[intfName].methods[member].outSig
		end
	-- Else an interface wasn't provided so find the first interface
	-- with a matching name and method signature
	else
		for intfKey, intfItem in pairs(svcObj.interfaces) do
			for methName, methItem in pairs(intfItem.methods) do
				if (methName == member) then
					if intfItem.methods[member].inSig == msg:getSignature() then
						handler = intfItem.methods[member].handler
						outSig = intfItem.methods[member].outSig
						break
					end
				end
			end
			if handler ~= nil then
				break
			end
		end
	end
	
	if (handler ~= nil) or (svcObj.defHandler ~= nil ) then
		if (outSig == nil) and intfName and svcObj.interfaces[intfName] then
			outSig = calcSignatureFromMetadata(member, "out",
								svcObj.interfaces[intfName].metadata)
		end
		context = newReplyContext(outSig, conn, msg)
		if handler ~= nil then
			status, result = pcall(handler, context, msg:getArgs())
		else
			status, result = pcall(svcObj.defHandler, context, intfName, member,
									msg:getArgsAsArray())
		end
		if not status then
			context:error(L2DBUS_ERROR_PROCESSING_REQUEST, result) 
		end
	-- Else there are no registered handlers to process this request
	else
		-- Indicate that it was *not* handled
		dbusResult = l2dbus.Dbus.HANDLER_RESULT_NOT_YET_HANDLED
	end
	
	return dbusResult
end


--- Constructs a new Service instance.
-- 
-- The constructor creates a Service instance. One or more D-Bus interfaces
-- can be associated with Service.
-- 
-- @tparam string objPath The D-Bus object path this service is implementing.
-- It must be a valid path.
-- @tparam bool introspectable If set to **true** this service will support
-- D-Bus introspection by implementing
-- <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-introspectable">
-- org.freedesktop.DBus.Introspectable</a> on behalf of the service. If
-- **false** then the resulting service **cannot** be introspected.
-- @tparam func defaultHandler The default (global) handler for this object.
-- This is called if none of the associated interface handlers can process
-- a request. It is the request handler of *last resort*.
-- @treturn table A Service instance.
function M.new(objPath, introspectable, defaultHandler)
	verify(validate.isValidObjectPath(objPath), "invalid D-Bus object path")
	verifyTypesWithMsg("nil|function", "unexpected type for arg #3", defaultHandler)
	
	local svcObj = {
				defHandler = defaultHandler,
				interfaces = {},
				objInst = nil
				}
				
	svcObj.objInst = l2dbus.ServiceObject.new(
									objPath,
									globalHandler,
									svcObj)
	
	if not svcObj.objInst then
		svcObj = nil
	else
		svcObj = setmetatable(svcObj, Service)
		if introspectable then
			if not svcObj.objInst:addInterface(l2dbus.Introspection.new()) then
				svcObj = nil
			end
		end
	end
	
	return svcObj
end

--- Converts a D-Bus XML description of an interface to a Lua equivalent.
-- 
-- This function takes a D-Bus XML introspection description of an interface
-- and converts it to a Lua table representation that can be used to register
-- the interface with a D-Bus service. If the D-Bus XML description lists
-- multiple interfaces then **only** the interface with the specified
-- name will be decoded into an equivalent Lua table.
-- 
-- @tparam string intfName The D-Bus interface name to be converted. The
-- name should be defined in the XML description.
-- @tparam string xmlStr The D-Bus introspection XML for the interface.
-- @treturn table|nil A Lua table representation of the D-Bus XML interface
-- description. Returns **nil** if the XML could not be converted or
-- the interface name was not found.
function M.convertXmlToIntfMeta(intfName, xmlStr)
	-- Take either a full or partial D-Bus Wire-protocol description and convert
	-- it to a structure that the lower-level l2dbus interface object can
	-- parse and interpret.
	local node = xml.parse(xmlStr)
	local interfaces = {}
    local intfNode = nil
    for item in node:childtags() do
        if item.tag ~= "interface" then
            intfNode = node
            break
        end
        if item.attr.name == intfName then
            intfNode = item
            break
        end
    end
    
    if intfNode then
	    local methods = {}
	    local signals = {}
	    local properties = {}
        for item in intfNode:childtags() do
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
                    table.insert(methods, {name = item.attr.name, args = args})
                else
                    table.insert(signals, {name = item.attr.name, args = args})
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
                table.insert(properties, {name=item.attr.name,
                						sig=item.attr.type,
                						access=access})
            end
        end
		table.insert(interfaces, { interface = intfName,
										   methods = methods,
			                               signals = signals,
			                               properties = properties})
	end
	
    return (#interfaces > 0) and interfaces[1] or nil
end

--- The Lua Table Representation of a D-Bus Interface Description.
-- </p>
-- The metadata describing a D-Bus interface can be specified using
-- one of two different formats. This first form is the direct D-Bus XML
-- introspection <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#introspection-format">description</a>.
-- The other format describes the D-Bus interface using a Lua table.
-- Internally, the XML format is always transformed into the more efficiently
-- manipulated Lua table-based representation described here.
-- </p>
-- An example Lua table representation of a D-Bus interface is shown below:
-- 
-- 		{
-- 			interface = "org.example.Interface",
--			properties = {					-- Array of properties
--				{							-- IntrospectProp[1] (Prop1)
--					name = "Prop1",
--					sig = "s",
--					access = "rw"
--				},
--				...							-- IntrospectProp[N] (PropN)
--			},
--			signals = {						-- Array of signals
--				{							-- IntrospectItem[1] (Signal1)
--					name = "Signal1",
--					args = {				-- Signal1 arguments
--						{					-- IntrospectArg[1]
--							sig = "s",
--							name = "arg1",
--						},
--						...					-- IntrospectArg[N]
--					}
--				},
--				...							-- IntrospectItem[N] (SignalN)
--			},
--			methods = {						-- Array of methods
--				{							-- IntrospectItem[1] (Method1)
--					name = "Method1",
--					args = {				-- Method1 arguments
--						{					-- IntrospectArg[1]
--							sig = "s",
--							name = "arg1",
--							dir = "in"
--						},
--						...					-- IntrospectArg[N]
--					}
--				},
--				...							-- IntrospectItem[N] (MethodN)
--			}
--		}
--
-- The key fields for the Lua table include:
-- </p>		
-- @field interface The valid D-Bus interface name
-- @field methods An array of methods of type @{l2dbus.Interface.IntrospectItem|IntrospectItem}
-- @field signals An array of signals of type @{l2dbus.Interface.IntrospectItem|IntrospectItem}
-- @field properties An array of properties of type @{l2dbus.Interface.IntrospectProp|IntrospectProp}
-- @table DbusInterfaceMetadata


--- Service
-- @type Service


--- Attaches a D-Bus service with associated object path to the connection.
-- 
-- This method is used to attach and register the service's object path
-- with the D-Bus connection. Once attached the service can receive
-- messages sent to its object path.
-- 
-- @within Service
-- @tparam userdata conn The D-Bus connection.
-- @treturn bool Returns **true** if the service is attached and **false**
-- otherwise.
-- @function attach
function Service:attach(conn)
	verify("userdata" == type(conn))
	return conn:registerServiceObject(self.objInst)
end


--- Detach a D-Bus service with associated object path from the connection.
-- 
-- This method is used to detach and unregister the service's object path
-- from the D-Bus connection. Once detached the service will no longer receive
-- messages sent to its object path.
-- 
-- @within Service
-- @tparam userdata svc The Service instance.
-- @tparam userdata conn The D-Bus connection.
-- @treturn bool Returns **true** if the service is detached and **false**
-- otherwise.
-- @function detach
function Service:detach(conn)
	verify("userdata" == type(conn))
	return conn:unregisterServiceObject(self.objInst)
end


--- Adds an interface to the D-Bus service.
-- 
-- This method is used to add the interface described by the provided
-- metadata to the service. If this service is introspectable then the
-- interface will appear to be implemented by the service with the associated
-- object path. The metadata can be in one of two formats: D-Bus 
-- introspection XML or a Lua table equivalent. The function
-- @{convertXmlToIntfMeta} can be used to convert XML to the Lua equivalent
-- but is typically unnecessary for this method.
-- </br>
-- If the interface that is being added contains D-Bus properties then
-- the D-Bus property interface <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-properties">
-- org.freedesktop.DBus.Properties</a> will automatically be added to this
-- service. It is required that handlers for the **Get**, **Set**, and
-- **GetAll** methods will be registered to handle inquires on the object
-- properties.
-- 
-- @within Service
-- @tparam userdata svc The Service instance.
-- @tparam string name The D-Bus name of the interface being added to the
-- service. It must be a valid D-Bus interface name.
-- @tparam string|table metadata The meta-data description of the D-Bus
-- interface. This can either be the D-Bus XML introspection description or
-- the equivalent Lua table @{DbusInterfaceMetadata|representation}. If the
-- D-Bus XML description lists multiple interfaces then **only** the interface
-- with the specified name will be decoded into an equivalent Lua table
-- representation.
-- @treturn bool Returns **true** if the interface is added successfully and
-- **false** otherwise.
-- @function addInterface
function Service:addInterface(name, metadata)
	verify(validate.isValidInterface(name), "invalid D-Bus interface name")
	-- If the metadata is a string then we'll assume it's D-Bus XML formatted
	-- data that can be converted to an equivalent Lua table.
	if "string" == type(metadata) then
		metadata = M.convertXmlToIntfMeta(name, metadata)
	end
	verify("table" == type(metadata))
	
	local isAdded = false
	local status = true
	-- Create a lower level interface
	local intfInst = l2dbus.Interface.new(name, nil, nil)
	if intfInst then
		if metadata.methods then 
			status = pcall(intfInst.registerMethods, intfInst, metadata.methods)
		end
		if status and metadata.signals then
			status = pcall(intfInst.registerSignals, intfInst, metadata.signals)
		end
		if status and metadata.properties then
			status = pcall(intfInst.registerProperties, intfInst, metadata.properties)
		end
		
		-- Add it to the lower-level service object
		if status and self.objInst:addInterface(intfInst) then
			self.interfaces[name] = { intfInst = intfInst,
									metadata = metadata,
									methods = {}}
			isAdded = true
		end
		
		-- If this interface has properties and the D-Bus property
		-- interface has not been registered yet then ...
		
		-- A potentially recursive call to add the DBus Property
		-- interface to this service object. This recursive call
		-- should work since the D-Bus Property interface has no
		-- properties itself.
		if (metadata.properties and #metadata.properties > 0) and
			(self.interfaces[DBUS_PROPERTIES_INTERFACE_NAME] == nil) then
			if not self:addInterface(DBUS_PROPERTIES_INTERFACE_NAME,
				DBUS_PROPERTIES_INTERFACE_METADATA) then
				-- Back-out the registration since we couldn't add
				-- the D-Bus Property interface
				isAdded = false
				self.objInst:removeInterface(intfInst)
				self.interfaces[name] = nil
			end
		end
	end
	
	-- If the interface wasn't added to the
	-- object successfully then ...
	if intfInst and not isAdded then
		intfInst:clearMethods()
		intfInst:clearSignals()
		intfInst:clearProperties()
	end
	
	return isAdded
end


--- Removes an interface from the D-Bus service.
-- 
-- This method removes the named D-Bus interface from the service. Handlers
-- for this interface will no longer be called. 
-- 
-- @within Service
-- @tparam userdata svc The Service instance.
-- @tparam string name The D-Bus interface name to remove.
-- @treturn bool Returns **true** if the interface is removed and **false**
-- otherwise.
-- @function removeInterface
function Service:removeInterface(name)
	verify(validate.isValidInterface(name), "invalid D-Bus interface name")
	local isRemoved = false
	if self.interfaces[name] then
		if self.objInst:removeInterface(self.interfaces[name].intfInst) then
			self.interfaces[name] = nil
			isRemoved = true
		end
	end
	return isRemoved
end


--- Registers a handler for a D-Bus interface/method combination.
-- 
-- This method registers a handler that will be called when a service
-- request directed to a given object path/interface/method combination is
-- received.
-- </br>
-- The signature of the handler function will be of the form:
-- 
-- 		function onRequest(ctx, arg1, arg2, ..., argN)
-- 
-- Where the first argument of the handler will **always** be a context
-- instance that can used to reply either immediately or at some point
-- int the  future (outside the scope of the handler if a reference to
-- the context is maintained). A Lua error may be thrown if an error is
-- encountered registering a handler. 
-- 
-- @within Service
-- @tparam userdata svc The Service instance.
-- @tparam string intfName The D-Bus interface name to associate the handler.
-- @tparam string methodName The D-Bus method name assocated with the handler.
-- @tparam func handler The handler function which will receive the message. If
-- a handler for this interface/method combination was already specified then
-- it will be replaced by this handler.
-- @function registerMethodHandler
function Service:registerMethodHandler(intfName, methodName, handler)
	verify(validate.isValidInterface(intfName), "invalid D-Bus interface name")
	verify(validate.isValidMember(methodName), "invalid D-Bus method name")
	verify("function" == type(handler))
	
	if self.interfaces[intfName] == nil then
		error("interface unknown to this service object: " .. intfName)
	end
	
	local numMethods = 0
	if self.interfaces[intfName].metadata.methods then
		numMethods = #self.interfaces[intfName].metadata.methods
	end
	
	local methodExists = false
	for idx = 1, numMethods do
		if self.interfaces[intfName].metadata.methods[idx].name == methodName  then
			methodExists = true
			break	
		end
	end
	
	if not methodExists then
		error("interface does not have method: " .. methodName)
	end
	
	-- This will replace any previous handler that might have
	-- already been assigned
	self.interfaces[intfName].methods[methodName] =
				{
				handler = handler,
				inSig = calcSignatureFromMetadata(methodName,
								"in",
								self.interfaces[intfName].metadata),
				outSig = calcSignatureFromMetadata(methodName,
								"out",
								self.interfaces[intfName].metadata)
				}
end


--- Unregisters a handler for D-Bus interface/method handler combination.
-- 
-- This method disassociates the method handler from the method so that
-- it will no longer be called when a matching method request is received
-- by the service. If the D-Bus interface name is unknown a Lua error
-- will be thrown.
-- 
-- @within Service
-- @tparam userdata svc The Service instance.
-- @tparam string intfName The D-Bus interface name owning the method.
-- @tparam string methodName The name of the D-Bus method to unregister
-- the handler.
-- @treturn bool Returns **true** if the handler is removed and **false**
-- otherwise.
-- @function unregisterMethodHandler
function Service:unregisterMethodHandler(intfName, methodName)
	verify(validate.isValidInterface(intfName), "invalid D-Bus interface name")
	verify(validate.isValidMember(methodName), "invalid D-Bus method name")
	
	if self.interfaces[intfName] == nil then
		error("interface '" .. intfName .. "' is unknown to this service object")
	end
	
	if self.interfaces[intfName].methods[methodName] then
		self.interfaces[intfName].methods[methodName] = nil
		return true
	else
		return false
	end
end


--- Provides a method to emit a signal on a specific connection.
-- 
-- This method provides a means to send a D-Bus signal with the given
-- interface name and signal name over the provided connection. Since a
-- service can exist on more than one connection (or D-Bus bus) the
-- actual connection on which the signal should be sent needs to be
-- specified explicitly. Connections *host* services and not vice-versa.
-- This method can throw Lua errors if the parameters are invalid or problems
-- occur encoding parameters or sending the signal.
-- </br>
-- **Note:** The interface specified by this method must be implemented
-- by this service. It is an error if the interface is unknown (e.g. an
-- arbitrary interface name cannot be specified). Also, signals are only
-- queued to be sent and are not immediately delivered. Call the
-- @{l2dbus.Connection.flush|flush} method on the connection to block until the
-- message has been sent. 
-- 
-- @within Service
-- @tparam userdata svc The Service instance.
-- @tparam userdata conn The D-Bus connection on which to emit the signal.
-- @tparam string intfName The D-Bus interface name owning the signal.
-- @tparam string signalName The name of the D-Bus signal.
-- @tparam any ... The list of arguments associated with the signal being
-- sent. The signature of the signal (as specified in the D-Bus interface
-- description) is used to map the Lua types to their underlying D-Bus types.
-- @treturn bool Returns **true** if the signal is enqueued to be sent and
-- **false** otherwise.
-- @treturn number Returns the serial number of the enqueued signal or
-- zero (0) if it cannot be queued.
-- @function emit
function Service:emit(conn, intfName, signalName, ...)
	verify(validate.isValidInterface(intfName), "invalid D-Bus interface name")
	verify(validate.isValidMember(signalName), "invalid D-Bus method name")
	
	if self.interfaces[intfName] == nil then
		error("interface '" .. intfName .. "' is unknown to this service object")
	end

	local nSignals = 0
	if self.interfaces[intfName].metadata.signals then
		nSignals = #self.interfaces[intfName].metadata.signals
	end
	local signature = ""
	for sigIdx = 1, nSignals do
		local sigItem = self.interfaces[intfName].metadata.signals[sigIdx]
		if sigItem.name == signalName then
			local nArgs = #sigItem.args
			for argIdx = 1, nArgs do
				signature = signature .. sigItem.args[argIdx].sig
			end
			break			
		end
	end
		
	local msg = l2dbus.Message.newSignal(self.objInst:path(), intfName,
										signalName)
	msg:addArgsBySignature(signature, ...)
	
	-- Add the arguments to the message
	return conn:send(msg)	
end


--- ReplyContext
-- @type ReplyContext


--
-- Set up the metadata information for the ReplyContext class
--
local ReplyContext = { __type = "l2dbus.lua.reply_context" }
ReplyContext.__index = ReplyContext

-- Constructor for a ReplyContext.
--
-- Creates a new reply context which can be used to reply to a request
-- at a later time. This is a "private" function that should only
-- be called (internally) by the Service's request dispatcher.
--
-- @within ReplyContext
-- @tparam string outSig The output/reply signature.
-- @tparam userdata conn The L2DBUS connection.
-- @tparam userdata msg The request message.
-- @treturn table The reply context.
-- @function newReplyContext
newReplyContext = function(outSig, conn, msg)
	local context = {
					outSignature = outSig,
					conn = conn,
					msg = msg
					}
	
	return setmetatable(context, ReplyContext)
end


--- Returns the D-Bus connection associated with the context.
--
-- Returns the D-Bus connection on which the original request message
-- arrived. This will be the connection on which the response is sent.
--
-- @within ReplyContext
-- @tparam table ctx The reply context.
-- @treturn userdata The D-Bus connection associated with the context.
-- @function getConnection
function ReplyContext:getConnection()
	return self.conn
end


--- Returns the request message associated with the context.
--
-- Returns the original D-Bus request message for which there should
-- be a reply.
--
-- @within ReplyContext
-- @tparam table ctx The reply context.
-- @treturn userdata The D-Bus request message associated with the context.
-- @function getMessage
function ReplyContext:getMessage()
	return self.msg
end


--- Returns an indication of whether the request expects a reply message.
--
-- The method returns a boolean value indicating whether or not the
-- original request expects a reply (or not). If no reply is needed/expected
-- then message traffic can be reduced by not sending a reply. Typically
-- most requests expect a reply. Technically, even if a reply is not needed
-- it is not an error to send one anyway. The client will likely ignore the
-- reply.
--
-- @within ReplyContext
-- @tparam table ctx The reply context.
-- @treturn bool Returns **true** if the request expects a reply, **false**
-- if a reply is **not** needed.
-- @function needsReply
function ReplyContext:needsReply()
	return not self.msg:getNoReply()
end


--- Returns a response to the associated method request.
--
-- The method queues the response to a request associated with the context.
-- The parameters are marshalled into a response message which is then
-- enqueued into the out-going message queue. If it's critical that the
-- response is delivered immediately (rather than during the next iteration
-- of the dispatch loop) call the @{l2dbus.Connection.flush|flush} method on
-- the connection to block until the response message has been sent. It is
-- considered an error to respond to the same request more than once. An
-- @{error} reply is considered a response as well.
--
-- @within ReplyContext
-- @tparam table ctx The reply context.
-- @tparam any ... The list of arguments associated with the response. The
-- signature of these arguments is taken from the D-Bus interface specification
-- for the return (out) parameters. This signature is used to guide the
-- conversion from Lua types to D-Bus types.
-- @treturn bool Returns **true** if the response is enqueued to be sent and
-- **false** otherwise.
-- @treturn number Returns the serial number of the enqueued response message
-- or zero (0) if it cannot be queued.
-- @function reply
function ReplyContext:reply(...)
	local replyMsg = l2dbus.Message.newMethodReturn(self.msg)
	local intfName = self.msg:getInterface()
	-- If an output signature is available then ...
	if self.outSignature then
		replyMsg:addArgsBySignature(self.outSignature, ...)
	-- Else no interface was specified in the request message
	else
		-- Make our best guess encoding thing correctly
		replyMsg:addArgs(...)
	end
	
	-- Return true/serial # or false/nil
	return self.conn:send(replyMsg)
end


--- Returns an error response to the associated method request.
--
-- The method queues an error response to the request associated with the
-- context. The error is enqueued into the out-going message queue. If it's
-- critical that the D-Bus error is delivered immediately (rather than during
-- the next iteration of the dispatch loop) call the
-- @{l2dbus.Connection.flush|flush} method on the connection to block until the
-- response message has been sent. An
-- @{error} reply is considered a valid
-- response to a request and fullfills the request. An additional response is
-- unnecessary.
--
-- @within ReplyContext
-- @tparam table ctx The reply context.
-- @tparam string errName The D-Bus error name.
-- @tparam string|nil errMsg An optional error message.
-- @treturn bool Returns **true** if the error is enqueued to be sent and
-- **false** otherwise.
-- @treturn number Returns the serial number of the enqueued error message
-- or zero (0) if it cannot be queued.
-- @function error
function ReplyContext:error(errName, errMsg)
	verify(validate.isValidInterface(errName), "invalid D-Bus error name")
	local errorMsg = l2dbus.Message.newError(self.msg, errName, errMsg)
	-- Return true/serial # or false/nil
	return self.conn:send(errorMsg)
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
if type(package.loaded[(...)]) ~= "userdata" then
    -- The module is being run as a program
    main(arg)
else
    -- The module is being loaded rather than run
    return M
end