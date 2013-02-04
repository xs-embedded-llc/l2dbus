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
@brief          Provides a proxy interface for a service.
*****************************************************************************
--]]

local core = require("l2dbus_core")
local class = require("l2dbus.30log")
local M = { }

-- Called when this module is run as a program
local function main(arg)
    print("Module: " .. string.match(arg[0], "^(.+)%.lua"))
    local info = core.getVersion()
    print("L2DBUS Version: " .. info.l2dbusVerStr)
    print("CDBUS Version: " .. info.cdbusVerStr)
    print(string.format("D-Bus Version: %d.%d.%d",
    		info.dbusMajor, info.dbusMinor, info.dbusRelease))
    print("Author: " .. info.author)
    print(info.copyright)
end


M.Proxy = class()

function M.Proxy:__init()
end

function M.Proxy:parseXml(xmlStr)
	local status, pl, parse
	status, pl = pcall(require, "pl.xml")
	if not status then
		error("Penlight parser is required to decode D-Bus introspection XML")
	else
		parse = pl.parse
	end
	
	local doc = parse(xmlStr)
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
	                    table.insert(args, {name = arg.attr.name, sig=arg.attr.type, dir=arg.attr.direction})
	                else
	                    table.insert(args, {name = arg.attr.name, sig=arg.attr.type, dir="out"})
	                end
	            end
	            if item.tag == "method" then
	                table.insert(methods, {name=item.attr.name, args=args})
	            else
	                table.insert(signals, {name=item.attr.name, args=args})
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
	            table.insert(properties, {name=item.attr.name, sig=item.attr.type, access=access})
	        end
	    end
	    table.insert(interfaces, {name=interface.attr.name,
	                                methods = methods,
	                                signals = signals,
	                                properties = properties})
	end
	
    return interfaces	
end

-- Determine the context in which the module is used
if type(package.loaded[(...)]) ~= "userdata" then
    -- The module is being run as a program
    main(arg)
else
    -- The module is being loaded rather than run
    return M
end