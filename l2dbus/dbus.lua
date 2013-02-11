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
@file           dbus.lua
@author         Glenn Schmottlach
@brief          Provides a proxy interface to the D-Bus daemon.
*****************************************************************************
--]]

local l2dbus = require("l2dbus_core")
local xml = require("l2dbus.xml")
local validate = require("l2dbus.validate")
local proxy = rquire("l2dbus.proxy")

local verifyTypes			=	validate.verifyTypes
local verifyTypesWithMsg	=	validate.verifyTypesWithMsg
local verify				=	validate.verify

local M = { }
local DbusController = { __type = "l2dbus.dbus_controller" }
DbusController.__index = DbusController

local DBUS_INTROSPECT_AS_XML =
[[
<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="org.freedesktop.DBus.Introspectable">
    <method name="Introspect">
      <arg name="data" direction="out" type="s"/>
    </method>
  </interface>
  <interface name="org.freedesktop.DBus">
    <method name="Hello">
      <arg direction="out" type="s"/>
    </method>
    <method name="RequestName">
      <arg direction="in" type="s"/>
      <arg direction="in" type="u"/>
      <arg direction="out" type="u"/>
    </method>
    <method name="ReleaseName">
      <arg direction="in" type="s"/>
      <arg direction="out" type="u"/>
    </method>
    <method name="StartServiceByName">
      <arg direction="in" type="s"/>
      <arg direction="in" type="u"/>
      <arg direction="out" type="u"/>
    </method>
    <method name="UpdateActivationEnvironment">
      <arg direction="in" type="a{ss}"/>
    </method>
    <method name="NameHasOwner">
      <arg direction="in" type="s"/>
      <arg direction="out" type="b"/>
    </method>
    <method name="ListNames">
      <arg direction="out" type="as"/>
    </method>
    <method name="ListActivatableNames">
      <arg direction="out" type="as"/>
    </method>
    <method name="AddMatch">
      <arg direction="in" type="s"/>
    </method>
    <method name="RemoveMatch">
      <arg direction="in" type="s"/>
    </method>
    <method name="GetNameOwner">
      <arg direction="in" type="s"/>
      <arg direction="out" type="s"/>
    </method>
    <method name="ListQueuedOwners">
      <arg direction="in" type="s"/>
      <arg direction="out" type="as"/>
    </method>
    <method name="GetConnectionUnixUser">
      <arg direction="in" type="s"/>
      <arg direction="out" type="u"/>
    </method>
    <method name="GetConnectionUnixProcessID">
      <arg direction="in" type="s"/>
      <arg direction="out" type="u"/>
    </method>
    <method name="GetAdtAuditSessionData">
      <arg direction="in" type="s"/>
      <arg direction="out" type="ay"/>
    </method>
    <method name="GetConnectionSELinuxSecurityContext">
      <arg direction="in" type="s"/>
      <arg direction="out" type="ay"/>
    </method>
    <method name="ReloadConfig">
    </method>
    <method name="GetId">
      <arg direction="out" type="s"/>
    </method>
    <signal name="NameOwnerChanged">
      <arg type="s"/>
      <arg type="s"/>
      <arg type="s"/>
    </signal>
    <signal name="NameLost">
      <arg type="s"/>
    </signal>
    <signal name="NameAcquired">
      <arg type="s"/>
    </signal>
  </interface>
</node>
]]

--
-- Forward method declarations
--


function M.new(conn)
	verify(type(conn) == "userdata", "invalid connection")
	local dbusCtrl = {
				ctrl = proxy.new(conn, l2dbus.Dbus.SERVICE_DBUS, l2dbus.Dbus.PATH_DBUS)
				}
					
	return setmetatable(dbusCtrl, DbusController)
end


function DbusController:bind(doIntrospect)
	if doIntrospect then
		return self.ctrl:bind()
	else
		return self.ctrl:bindNoIntrospect(DBUS_INTROSPECT_AS_XML, true)
	end
end


function DbusController:unbind()
	self.ctrl:unbind()
end


function DbusController:getIntrospectionData()
	return self.ctrl:getIntrospectionData()
end


function DbusController:getProxy(interface)
	return self.ctrl:getProxy(l2dbus.Dbus.INTERFACE_DBUS)	
end


function DbusController:setTimeout(timeout)
	self.ctrl:setTimeout(timeout)
end


function DbusController:getTimeout()
	return self.ctrl:getTimeout()
end


function DbusController:setBlockingMode(mode)
	self.ctrl:setBlockingMode(mode)
end


function DbusController:getBlockingMode()
	return self.ctrl:getBlockingMode()
end

function DbusController:connectSignal(sigName, handler)
	return self.ctrl:connectSignal(l2dbus.Dbus.INTERFACE_DBUS, sigName, handler)
end


function DbusController:disconnectSignal(hnd)	
	return self.ctrl:disconnnectSignal(hnd)
end


function DbusController:disconnectAllSignals()
	self.ctrl:disconnectAllSignals()
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