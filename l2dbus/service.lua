--[[
*****************************************************************************
Project         l2dbus
(c) Copyright   2013 XS-Embedded LLC
                 All rights reserved

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*****************************************************************************
*****************************************************************************
@file           service.lua
@author         Glenn Schmottlach
@brief          Provides an object service framework for D-Bus services.
*****************************************************************************
--]]

local l2dbus = require("l2dbus_core")
local xml = require("l2dbus.xml")
local validate = require("l2dbus.validate")

local verifyTypes			=	validate.verifyTypes
local verifyTypesWithMsg	=	validate.verifyTypesWithMsg
local verify				=	validate.verify

local M = { }
local ServiceObject = { __type = "l2dbus.lua.service_object" }
ServiceObject.__index = ServiceObject

local L2DBUS_ERROR_PROCESSING_REQUEST = "org.l2dbus.error.ProcessingRequest"
local DBUS_PROPERTIES_INTERFACE_NAME = "org.freedesktop.DBus.Properties"
local DBUS_PROPERTIES_INTERFACE_METADATA =
{
  {
    properties = {
    },
    signals = {
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
    interface = "org.freedesktop.DBus"
  }
}

--
-- Forward method declarations
--
local newReplyContext


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
								self.svcObj.interfaces[intfName].metadata)
		end
		context = newReplyContext(outSig, conn, msg)
		if handler ~= nil then
			status, result = pcall(handler, context, msg:getArgs())
		else
			status, result = pcall(svcObj.defHandler, context, intfName, member, msg:getArgsAsArray())
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
		svcObj = setmetatable(svcObj, ServiceObject)
		if introspectable then
			if not svcObj.objInst:addInterface(l2dbus.Introspection.new()) then
				svcObj = nil
			end
		end
	end
	
	return svcObj
end


ServiceObject:attach(conn)
	verify("userdata" == type(conn))
	return conn:registerServiceObject(self.objInst)
end


ServiceObject:detach(conn)
	verify("userdata" == type(conn))
	return conn:unregisterServiceObject(self.objInst)
end


ServiceObject:addInterface(name, metadata)
	verify(validate.isValidInterface(name), "invalid D-Bus interface name")
	verify("table" == type(metadata))
	local isAdded = false
	-- Create a lower level interface
	local intfInst = l2dbus.Interface.new(name, nil, nil)
	if intfInst and
		intfInst:registerMethods(metadata.methods) and
		intfInst:registerSignals(metadata.signals) and
		intfInst:registerProperties(metadata.properties) then
		-- Add it to the lower-level service object
		if self.objInst:addInterface(intfInst) then
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
		if (#metadata.properties > 0) and
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


ServiceObject:removeInterface(name)
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


ServiceObject:registerMethodHandler(intfName, methodName, handler)
	verify(validate.isValidInterface(intfName), "invalid D-Bus interface name")
	verify(validate.isValidMember(methodName), "invalid D-Bus method name")
	verify("function" == type(handler))
	
	if ( self.interfaces[intfName] == nil )
		error("interface unknown to this service object: " .. intfName)
	end
	
	local numMethods = #self.interfaces[intfName].metadata.methods
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


ServiceObject:unregisterMethodHandler(intfName, methodName)
	verify(validate.isValidInterface(intfName), "invalid D-Bus interface name")
	verify(validate.isValidMember(methodName), "invalid D-Bus method name")
	
	if ( self.interfaces[intfName] == nil )
		error("interface '" .. intfName .. "' is unknown to this service object")
	end
	
	if self.interfaces[intfName].methods[methodName] then
		self.interfaces[intfName].methods[methodName] = nil
		return true
	else
		return false
	end
end


ServiceObject:emit(conn, intfName, signalName, ...)
	verify(validate.isValidInterface(intfName), "invalid D-Bus interface name")
	verify(validate.isValidMember(signalName), "invalid D-Bus method name")
	
	if ( self.interfaces[intfName] = nil )
		error("interface '" .. intfName .. "' is unknown to this service object")
	end

	local nSignals = #self.interfaces[intfName].metadata.signals
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
		
	local msg = l2dbus.Message.newSignal(self.objInst:path(), intfName, signalName)
	msg:addArgsBySignature(signature, ...)
	
	-- Add the arguments to the message
	return conn:send(msg)
	
end


ServiceObject:convertXmlToIntfMeta(intfName, xmlStr)
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
            elseif item.tag == "property" thensignature = calcSignatureFromMetadata(msg:getMember(), "out",
								self.svcObj.interfaces[intfName].metadata)
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


local ReplyContext = { __type = "l2dbus.lua.reply_context" }
ReplyContext.__index = RequestContext

newReplyContext = function(outSig, conn, msg)
	local context = {
					outSignature = outSig,
					conn = conn,
					msg = msg
					}
	
	return setmetatable(context, ReplyContext)
end


ReplyContext:needsReply()
	return self.msg:getNoReply()
end


ReplyContext:reply(...)
	local replyMsg = l2dbus.Message.newMethodReturn(self.msg)
	local intfName = msg:getInterface()
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


ReplyContext:error(errName, errMsg)
	verify(validate.isValidInterface(errName), "invalid D-Bus error name")
	local errorMsg = l2dbus.Message.newError(self.msg, errName, errMsg)
	-- Return true/serial # or false/nil
	return self.conn:send(replyMsg)
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