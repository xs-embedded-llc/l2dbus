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
local ServiceObject = { __type = "l2dbus." }
ServiceObject.__index = ServiceObject

--
-- Forward method declarations
--


local function globalHandler(objInst, conn, msg, svcObj)
	-- TODO Loop through looking for specific handlers for the methods/properties
	-- and unmarshall the arguments, create a reply context, and call this handlers.
	-- anything that doesn't match goes to a client specified "global" handler but
	-- the arguments are left as an array of arguments.
end


function M.new(objPath, defaultHandler)
	verify(validate.isValidObjectPath(objPath), "invalid D-Bus object path")
	verify("function" == type(defaultHandler))
	
	local svcObj = {
				conn = nil,
				clientDefHandler = defaultHandler,
				isBound = false,
				interfaces = {}
				}
				
	svcObj.inst = l2dbus.ServiceObject.new(
									objPath,
									globalHandler,
									svcObj)
					
	return setmetatable(svcObj, ServiceObject)
end


ServiceObject:bind(conn)
	verify("userdata" == type(conn))
	if self.isBound then
		self:unbind(conn)
	end
	
	self.isBound = conn:registerServiceObject(self.inst)
	return self.isBound
end


ServiceObject:unbind(conn)
	verify("userdata" == type(conn))
	
	if self.isBound then
		if conn:unregisterServiceObject(self.inst) then
			self.isBound = false
		end
	end
end


ServiceObject:addInterface(name, metadata)
	verify(validate.isValidInterface(name), "invalid D-Bus interface name")
	verify("table" == type(metadata))
	local isAdded = false
	-- Create a lower level interface
	local intfInst = l2dbus.Interface.new(name, nil, nil)
	if intfInst.registerMethods(metadata.methods) and
		intfInst.registerSignals(metadata.signals) and
		intfInst.registerProperties(metadata.properties) then
		-- Add it to the lower-level service object
		if self.inst:addInterface(intfInst) then
			-- TODO We should probably make the handler tables
			-- weak so the functions aren't anchored. Also,
			-- should they support some kind of user data
			-- as well?
			self.interfaces[name] = { inst = intfInst,
									metadata = metadata,
									methHandlers = {},
									propHandler = {}}
			isAdded = true
		end
	end
	
	return isAdded
end


ServiceObject:removeInterface(name)
	verify(validate.isValidInterface(name), "invalid D-Bus interface name")
	local isRemoved = false
	if self.interfaces[name] then
		if self.inst:removeInterface(self.interfaces[name].inst) then
			self.interfaces[name] = nil
			isRemoved = true
		end
	end
	return isRemoved
end


ServiceObject:registerMethodHandler(intfName, methodName, handler)
	verify(validate.isValidInterface(intfName), "invalid D-Bus interface name")
	verify(validate.isValidMember(methName), "invalid D-Bus method name")
	verify("function" == type(handler))
	
	-- TODO Need to finish this function. Should I add a user data parameter to
	-- this call and store it too? Perhaps currying it would be an alternative
	-- solution?
end

ServiceObject:unregisterMethodHandler(intfName, methodName)
end


ServiceObject:registerPropertyHandler(intfName, propName, handler)
end


ServiceObject:unregisterPropertyHandler(intfName, propName)
end


ServiceObject:convertXmlToIntfMeta(intfName, xmlDoc)
	-- TODO take either a full or partial D-Bus Wire-protocol description and convert
	-- a specific interface.
end

ServiceObject:convertXmlToIntfMeta(intfName, xmlStr)
	-- Take either a full or partial D-Bus Wire-protocol description and convert
	-- it to a structure that the lower-level l2dbus interface object can
	-- parse.
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