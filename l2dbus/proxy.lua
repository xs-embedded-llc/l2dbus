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
@file           proxy.lua
@author         Glenn Schmottlach
@brief          Provides a proxy interface D-Bus services.
*****************************************************************************
--]]

local l2dbus = require("l2dbus_core")
local xml = require("l2dbus.xml")
local validate = require("l2dbus.validate")

local verifyTypes			=	validate.verifyTypes
local verifyTypesWithMsg	=	validate.verifyTypesWithMsg
local verify				=	validate.verify

local M = { }
local ProxyController = { __type = "l2dbus.proxy_controller" }
ProxyController.__index = ProxyController

--
-- Forward declarations
--
local newProxy


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
				timeout = l2dbus.Dbus.TIMEOUT_USE_DEFAULT
				}
					
	return setmetatable(proxyController, ProxyController)
end


function ProxyController:bind(interface)
	verify(validate.isValidInterface(interface), "invalid D-Bus interface")
	verify(self.conn:isConnected(), "not connected to the D-Bus bus")
	
	local metadata
	if not self.introspectData then
		local msg = l2dbus.Message.newMethodCall({destination=self.busName,
								path=self.objPath,
								interface=l2dbus.Dbus.INTERFACE_INTROSPECTABLE,
								method="Introspect"})
		local reply, errName, errMsg = self:sendMessage(msg)
		if not reply then
			return nil, errName, errMsg
		end
		metadata = self:parseXml(reply:getArgs())
	end
	return self:bindNoIntrospect(interface, metadata, false)
end

function ProxyController:bindNoIntrospect(interface, introspectData, asXml)
	verify(validate.isValidInterface(interface), "invalid D-Bus interface")
	verifyTypesWithMsg("string|table", "unexpected type for arg #2", introspectData)
	verifyTypesWithMsg("nil|boolean", "unexpected type for arg #3", asXml)
		
	if asXml then
		self.introspectData = self:parseXml(introspectData)
	else
		self.introspectData = introspectData
	end
	
	local metadata = self.introspectData[interface]	
	if not metadata then
		error("Interface '" .. interface .. "' not exposed by service")
	end
	
	return newProxy(self, metadata)
end


function ProxyController:unbind()
	self.introspectionData = nil
end


function ProxyController:getIntrospectionData()
	return self.introspectData
end


function ProxyController:setTimeout(timeout)
	verify("number" == type(timeout), "timeout must be a number")
	self.timeout = timeout
end


function ProxyController:getTimeout()
	return self.timeout
end


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


function ProxyController:disconnectSignal(hnd)
	local disconnected = self.conn:unregisterMatch(hnd)
	if disconnected then
		self.signalHnds[hnd] = nil
	end
	
	return disconnected
end


function ProxyController:disconnectAllSignals()
	for hnd,_ in pairs(self.signalHnds) do
		if not self.conn:unregisterMatch(hnd) then
			error("failed to disconnect signal (hnd=" .. tostring(hnd) .. ")")
		end
	end
	
	self.signalHnds = {}
end


function ProxyController:sendMessage(msg)
	verifyTypesWithMsg("userdata", "unexpected type for arg #1", msg)
	
	local function onNotifyReply(pending, co)
		coroutine.resume(co, pending:stealReply())
	end

	local reply = nil
	local errName = nil
	local errMsg = nil
	
	-- Determine if this function is called from a
	-- coroutine or from the main thread
	local co = coroutine.running()
	if co then
		local status, pending = self.conn:sendWithReply(msg, self.timeout)
		if not status then
			reply, errName, errMsg = nil, l2dbus.Dbus.ERROR_FAILED, "unknown failure"
		else
			pending:setNotify(onNotifyReply, co)
			reply = coroutine.yield()
			assert( pending:isCompleted() )
			if not reply then
				reply, errName, errMsg = nil, l2dbus.Dbus.ERROR_FAILED, "unknown failure"
			elseif l2dbus.Message.ERROR == reply:getType() then
				reply, errName, errMsg = nil, reply:getErrorName(), reply:getArgs()
			end
		end
	else
		reply, errName, errMsg = self.conn:sendWithReplyAndBlock(msg, self.timeout)
	end
	
	return reply, errName, errMsg
end


function ProxyController:sendMessageNoReply(msg)
	verifyTypesWithMsg("userdata", "unexpected type for arg #1", msg)
	msg:setNoReply(true)	
	return self.conn:send(msg)
end


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


local Proxy = 
{
	__type = "l2dbus.proxy",
	
	__index = function(proxy, member)
		local methodInfo = proxy.metadata.methods[member]
		local propInfo
		if not methodInfo then
			propInfo = proxy.metadata.properties[member]
		end
		
		if (methodInfo == nil) and (propInfo == nil) then
			error("unknown member: " .. tostring(member))
		end
		
		local memFunc = function(...)
			local msg = l2dbus.Message.newMethodCall({destination=proxy.ctrl.busName,
							path=proxy.ctrl.objPath,
							interface=proxy.metadata.interface,
							method=member})
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
				error(string.format("method argument mis-match : provided=%d needed=%d",
						#callingArgs, #inArgs))
			else
				for idx=1,#inArgs do
					msg:addArgsBySignature(inArgs[idx].sig, callingArgs[idx])
				end
			end
			local reply, errName, errMsg = proxy.ctrl:sendMessage(msg)
			if not reply then
				error(string.format("%s : %s", tostring(errName), tostring(errMsg)))
			end
			return reply:getArgs()
		end
		
		local propFunc = function()
			if not propInfo.access:find("r") then
				error(string.format("property '%s' is write-only", member))
			end
			
			if not proxy.ctrl.introspectData[l2dbus.Dbus.INTERFACE_PROPERTIES] then
				error(string.format("object '%s' does not support %s",
						proxy.ctrl.objPath, l2dbus.Dbus.INTERFACE_PROPERTIES))
			end
			
			local msg = l2dbus.Message.newMethodCall({destination=proxy.ctrl.busName,
							path=proxy.ctrl.objPath,
							interface=l2dbus.Dbus.INTERFACE_PROPERTIES,
							method="Get"})
			if not msg then
				error("unable to create D-Bus method call message")
			end
			
			msg:addArgsBySignature("s", proxy.metadata.interface)
			msg:addArgsBySignature("s", member)
			local reply, errName, errMsg = proxy.ctrl:sendMessage(msg)
			if not reply then
				error(string.format("%s : %s", tostring(errName), tostring(errMsg)))
			end
			
			return reply:getArgs()
		end
		
		-- Decide whether we need to return a function that calls a function
		-- or returns a value of a property
		if methodInfo then
			return memFunc
		else
			return propFunc()
		end	
	end;

	
	__newindex = function(proxy, name, value)
		local propInfo = proxy.metadata.properties[name]
		if not propInfo then
			error("unknown property: " .. tostring(name))
		end
		
		if not propInfo.access:find("w") then
			error(string.format("property '%s' is read-only", name))
		end
		
		if not proxy.ctrl.introspectData[l2dbus.Dbus.INTERFACE_PROPERTIES] then
			error(string.format("object '%s' does not support %s",
					proxy.ctrl.objPath, l2dbus.Dbus.INTERFACE_PROPERTIES))
		end
		
		local msg = l2dbus.Message.newMethodCall({destination=proxy.ctrl.busName,
						path=proxy.ctrl.objPath,
						interface=l2dbus.Dbus.INTERFACE_PROPERTIES,
						method="Set"})
		if not msg then
			error("unable to create D-Bus method call message")
		end
		
		msg:addArgsBySignature("s", proxy.metadata.interface)
		msg:addArgsBySignature("s", name)
		msg:addArgs(l2dbus.DbusTypes.Variant.new(value), "v" .. propInfo.sig)
		local reply, errName, errMsg = proxy.ctrl:sendMessage(msg)
		if not reply then
			error(string.format("%s : %s", tostring(errName), tostring(errMsg)))
		end
	end
}


newProxy = function (proxyCtrl, interfaceInfo)
	verifyTypesWithMsg("table", "unexpected type for arg #1", proxyCtrl)
	verifyTypesWithMsg("table", "unexpected type for arg #2", introspectInfo)
	
	proxy = {
			ctrl = proxyCtrl,
			metadata = interfaceInfo
			}
	
	return setmetatable(proxy, Proxy) 
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