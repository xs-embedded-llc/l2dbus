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
-- Forward method declarations
--
local newMethodProxy
local newPropertyProxy


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
				timeout = l2dbus.Dbus.TIMEOUT_USE_DEFAULT
				}
					
	return setmetatable(proxyController, ProxyController)
end


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
		if self:getBlockingMode() then
			self.introspectData = self:parseXml(reply:getArgs())
		else
			reply:block()
			local msg = reply:stealReply()
			self.introspectData = self:parseXml(msg:getArgs())
		end
	end
	return true
end


function ProxyController:bindNoIntrospect(introspectData, asXml)
	verifyTypesWithMsg("string|table", "unexpected type for arg #2", introspectData)
	verifyTypesWithMsg("nil|boolean", "unexpected type for arg #3", asXml)
		
	if asXml then
		self.introspectData = self:parseXml(introspectData)
	else
		self.introspectData = introspectData
	end	
	return true
end


function ProxyController:unbind()
	self.introspectionData = nil
end


function ProxyController:getIntrospectionData()
	return self.introspectData
end


function ProxyController:getProxy(interface)
	verify(validate.isValidInterface(interface), "invalid D-Bus interface")
	verify( self.introspectData, "controller is not bound to service object")

	local metadata = self.introspectData[interface]
	if not metadata then
		error("service object does not implement " .. interface)
	end
	
	return { m = newMethodProxy(self, metadata), p = newPropertyProxy(self, metadata) }	
end


function ProxyController:setTimeout(timeout)
	verify("number" == type(timeout), "timeout must be a number")
	self.timeout = timeout
end


function ProxyController:getTimeout()
	return self.timeout
end


function ProxyController:setBlockingMode(mode)
	self.blockingMode = mode and true or false
end


function ProxyController:getBlockingMode()
	return self.blockingMode
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
	
	local reply = nil
	local errName = nil
	local errMsg = nil
	
	if not self.blockingMode then
		local status, pending = self.conn:sendWithReply(msg, self.timeout)
		if not status then
			reply, errName, errMsg = nil, l2dbus.Dbus.ERROR_FAILED, "failed to send message"
		else
			reply, errName, errMsg = pending, nil, nil
		end
	-- Else we're making blocking calls
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


function ProxyController:waitForReply(pendingCall)
	verify("userdata" == type(pendingCall))
	
	local function onNotifyReply(pending, co)
		coroutine.resume(co, pending:stealReply())
	end
	
	local reply = nil
	local errName = nil
	local errMsg = nil
	local co = coroutine.running()
	if not co then
		error("cannot wait(yield) for a reply from the main thread")
	end
	
	if not pendingCall:isCompleted() then
		pendingCall:setNotify(onNotifyReply, co)
		reply = coroutine.yield()
		assert( pendingCall:isCompleted() )
	else
		reply = pendingCall:stealReply()
	end
	
	if not reply then
		reply, errName, errMsg = nil, l2dbus.Dbus.ERROR_FAILED, "no reply received"
	elseif l2dbus.Message.ERROR == reply:getType() then
		reply, errName, errMsg = nil, reply:getErrorName(), reply:getArgs()
	end
	
	return reply, errName, errMsg 
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
				error(string.format("method argument mis-match : provided=%d needed=%d",
						#callingArgs, #inArgs))
			else
				for idx=1,#inArgs do
					msg:addArgsBySignature(inArgs[idx].sig, callingArgs[idx])
				end
			end
			local reply, errName, errMsg = ctrl:sendMessage(msg)
			if not reply then
				error(string.format("%s : %s", tostring(errName), tostring(errMsg)))
			end
			if coroutine.running() == nil then
				return reply:getArgs()
			-- Else return the pending call
			else
				return reply	-- PendingCall object
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
			if not reply then
				error(string.format("%s : %s", tostring(errName), tostring(errMsg)))
			end
			
			-- If we're running in the main thread then ...
			if coroutine.running() == nil then
				return reply:getArgs()
			-- Else return the pending call
			else
				return reply	-- PendingCall object
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
			msg:addArgs(l2dbus.DbusTypes.Variant.new(value), "v" .. propInfo.sig)
			local reply = nil
			local errName = nil
			local errMsg = nil
			if noReplyNeeded then
				return ctrl:sendMessageNoReply(msg)
			else
				reply, errName, errMsg = ctrl:sendMessage(msg)
				if not reply then
					error(string.format("%s : %s", tostring(errName), tostring(errMsg)))
				end
				-- If we're running in the main thread then ...
				if coroutine.running() == nil then
					return reply:getArgs()
				-- Else return the pending call
				else
					return reply	-- PendingCall object
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
if type(package.loaded[(...)]) ~= "userdata" then
    -- The module is being run as a program
    main(arg)
else
    -- The module is being loaded rather than run
    return M
end