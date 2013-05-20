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
@file           init.lua
@author         Glenn Schmottlach
@brief          The core (low-level) l2dbus module.
*****************************************************************************
--]]

local l2dbus = require("l2dbus_core")

-- Try to load the Lua libev module
local gMainLoop = nil
local status, ev = pcall(require, "ev")
if status then
	-- If loaded successfully then we'll use the default main loop as
	-- the main loop for the dispatcher.
	gMainLoop = ev.Loop.default
	-- Call method to realize it so it's actually allocated
	gMainLoop:now()
end

-- Override the "core" Dispatcher constructor.
local coreDispatcherNew = l2dbus.Dispatcher.new
l2dbus.Dispatcher.new = function(loop)
	if loop == nil then
		loop = gMainLoop
	end
	
	return coreDispatcherNew(loop)
end


-- Documented in the l2dbus_core.c file.
l2dbus.getDefaultMainLoop = function()
	return gMainLoop
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
    return l2dbus
end