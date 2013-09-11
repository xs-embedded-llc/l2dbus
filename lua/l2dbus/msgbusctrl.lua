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
@file           msgbusctrl.lua
@author         Glenn Schmottlach
@brief          Provides a proxy interface to the D-Bus daemon.
*****************************************************************************
--]]

--- Message Bus Controller Module.
-- This module provides a controller for the D-Bus Message Bus which implements
-- the <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-bus">
-- org.freedesktop.DBus</a> interface. The controller provides methods to gain
-- access to the proxy interface of the Message Bus where methods can be called
-- and signals received from the service.
-- 
-- @module l2dbus.msgbusctrl
-- @alias M


local l2dbus = require("l2dbus_core")
local xml = require("l2dbus.xml")
local validate = require("l2dbus.validate")
local proxyCtrl = require("l2dbus.proxyctrl")

local verifyTypes			=	validate.verifyTypes
local verifyTypesWithMsg	=	validate.verifyTypesWithMsg
local verify				=	validate.verify

local M = { }
local MsgBusController = { __type = "l2dbus.msg_bus_controller" }
MsgBusController.__index = MsgBusController

local MSGBUS_INTROSPECT_AS_XML =
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



--- Constructs a new Message Bus Controller (MsgBusController) instance.
-- 
-- The constructor for a Message Bus Controller is used to call methods
-- and receive signals from the D-Bus Message Bus service.
-- @tparam userdata conn The @{l2dbus.Connection|Connection} to attach
-- the controller to.
-- @treturn table A MsgBusController instance.
function M.new(conn)
	verify(type(conn) == "userdata", "invalid connection")
	local msgBusCtrl = {
		ctrl = proxyCtrl.new(conn, l2dbus.Dbus.SERVICE_DBUS, l2dbus.Dbus.PATH_DBUS)
		}
					
	return setmetatable(msgBusCtrl, MsgBusController)
end


--- Message Bus Controller (MsgBusController)
-- @type MsgBusController

--- Binds the controller to the Message Bus.
-- 
-- This method may throw a Lua error if an exceptional (unexpected) error
-- occurs.
-- 
-- @within MsgBusController
-- @tparam userdata ctrl Message Bus controller instance.
-- @tparam bool doIntrospect If set to **true** the controller will make an
-- introspection call on the Message Bus service. If set to **false**
-- (the default) the controller will use cached introspection data
-- to generate the proxy interface.
-- @treturn true|nil Returns **true** if the binding operation succeeds
-- or **nil** on failure.
-- @treturn ?string|nil Returns an error name or **nil** if a name is
-- unavailable and the binding operation fails.
-- @treturn ?string|nil Returns an error message or **nil** if a message is
-- unavailable and the binding operation fails.
-- @function bind
function MsgBusController:bind(doIntrospect)
	if doIntrospect then
		return self.ctrl:bind()
	else
		return self.ctrl:bindNoIntrospect(MSGBUS_INTROSPECT_AS_XML)
	end
end


--- Unbinds the controller from the Message Bus.
-- 
-- Once unbound, methods on the Message Bus proxy should no longer be called.
-- 
-- @within MsgBusController
-- @tparam userdata ctrl Message Bus controller instance.
-- @function unbind
function MsgBusController:unbind()
	self.ctrl:unbind()
end


--- Returns the parsed introspection data for the Message Bus.
-- 
-- The returned Lua table is a representation of the D-Bus XML data in a
-- format that is understood by the proxy. Internally the proxy does **not**
-- maintain the original D-Bus XML description.
--
-- @tparam userdata ctrl Message Bus controller instance. 
-- @treturn table A Lua table containing the parsed introspection data for
-- the Message Bus.
-- 
-- @within MsgBusController
-- @function getIntrospectionData
function MsgBusController:getIntrospectionData()
	return self.ctrl:getIntrospectionData()
end


--- Returns the actual proxy for the Message Bus.
-- 
-- The Message Bus Controller controls the configuration and behavior of
-- the actual proxy implementation. The proxy object contains both the
-- method signatures and properties of the Message Bus service itself and
-- can be used to directly call methods or interrogate properties on the
-- remote service.
-- 
-- @tparam userdata ctrl Message Bus controller instance. 
-- @treturn table Returns the Message Bus proxy instance.
-- 
-- @within MsgBusController
-- @function getProxy
function MsgBusController:getProxy()
	return self.ctrl:getProxy(l2dbus.Dbus.INTERFACE_DBUS)	
end


--- Sets the timeout for accessing method/properties of the Message Bus.
-- 
-- The timeout (in milliseconds) controls how long the proxy will wait to
-- hear from the Message Bus service before timing out. All calls through
-- the proxy interface will use this *global* timeout when a D-Bus request
-- message is issued on the bus. The controller does not offer a per
-- method/property timeout but instead applies the timeout to all
-- future requests.
-- 
-- The timeout value may be set to two well-know *special*
-- values: @{l2dbus.Dbus.TIMEOUT_USE_DEFAULT|TIMEOUT_USE_DEFAULT} or
-- @{l2dbus.Dbus.TIMEOUT_INFINITE|TIMEOUT_INFINITE}
--
-- @tparam userdata ctrl Message Bus controller instance.
-- @tparam number timeout The time in milliseconds to set as the timeout
-- for future calls to the Message Bus. This value applies globally to all
-- method/property calls. The timeout value must be non-negative.
-- 
-- @within MsgBusController
-- @function setTimeout
function MsgBusController:setTimeout(timeout)
	self.ctrl:setTimeout(timeout)
end


--- Gets the global timeout value used by the Message Bus proxy.
-- 
-- Returns the timeout value used by the proxy when making method calls or
-- manipulating properties.
--
-- @tparam userdata ctrl Message Bus controller instance. 
-- @treturn number The timeout (in milliseconds) used by the proxy. It
-- may be one of the *special* values: @{l2dbus.Dbus.TIMEOUT_USE_DEFAULT|TIMEOUT_USE_DEFAULT}
-- or @{l2dbus.Dbus.TIMEOUT_INFINITE|TIMEOUT_INFINITE}.
-- 
-- @within MsgBusController
-- @function getTimeout
function MsgBusController:getTimeout()
	return self.ctrl:getTimeout()
end


--- Sets whether Message Bus proxy method calls expect/need a reply.
-- 
-- This method determines whether proxy calls to the Message Bus need a
-- response. Specifically, if set to **true**, out-going messages are marked to
-- indicate that a reply is not needed so that the Message Bus can decide
-- whether or not to reply to the message. The Message Bus can always reply
-- regardless of the flag but it means the near-end will ignore the reply. The
-- default setting for the controller is **false** which means replies are
-- expected from the far-end. If set to **true** out-going messages are marked
-- to indicate a reply is not needed. This setting applies to all subsequent
-- proxy *method* calls made *after* the value is changed. The behavior of
-- individual proxy method calls can only be controlled by calling this method
-- *before* making the method call on the Message Bus proxy. 
-- 
-- @within MsgBusController
-- @tparam table ctrl The Message Bus controller instance.
-- @tparam bool mode Set to **true** if no reply is needed/expected,
-- **false** to indicate a reply is expected.
-- @function setProxyNoReplyNeeded
function MsgBusController:setProxyNoReplyNeeded(noReply)
	self.ctrl:setProxyNoReplyNeeded(noReply)
end


--- Indicates whether Message Bus proxy calls need/expect a reply.
-- 
-- This method returns an indication of whether Message Bus proxy method calls
-- expect a reply. If this returns **true** then no reply is needed and proxy
-- method calls will **not** wait to hear the reply to a request. If **false**
-- is returned (the default behavior) then it is expected that proxy method
-- calls will return a reply message.
-- 
-- @within MsgBusController
-- @tparam table ctrl The Message Bus controller instance.
-- @treturn bool Returns **true** if no reply is expected from the Message Bus,
-- **false** (the default) if every proxy request expects a reply.
-- @function getProxyNoReplyNeeded
function MsgBusController:getProxyNoReplyNeeded()
	return self.ctrl:getProxyNoReplyNeeded()
end

--- Sets whether proxy calls to the Message Bus are blocking or unblocking.
-- 
-- This option determines whether calls made through the proxy are blocking
-- (synchronous) or non-blocking (asynchronous) calls. A blocking call will
-- block waiting for the reply or a timeout error before returning. A
-- non-blocking call will return a @{l2dbus.PendingCall|PendingCall} object
-- which can be used to wait for a reply or for registering a handler that will
-- be called when the reply or error is returned. Once set this option applies
-- globally to all methods/properties of the Message Bus proxy.
-- 
-- @tparam userdata ctrl Message Bus controller instance. 
-- @tparam bool mode Set to **true** if proxy calls should be blocking and
-- **false** for non-blocking (asynchronous) calls. 
-- @within MsgBusController
-- @function setBlockingMode
function MsgBusController:setBlockingMode(mode)
	self.ctrl:setBlockingMode(mode)
end


--- Retrieves whether proxy calls are blocking or non-blocking.
-- 
-- Returns the current option for call to methods/properties of the
-- Message Bus service.
--
-- @tparam userdata ctrl Message Bus controller instance. 
-- @treturn bool Returns **true** for blocking mode and **false** for
-- non-blocking mode. 
-- @within MsgBusController
-- @function getBlockingMode
function MsgBusController:getBlockingMode()
	return self.ctrl:getBlockingMode()
end


--- Connects a handler for a specific Message Bus signal.
-- 
-- This method registers a signal handler for the specified Message Bus
-- signal name. When a matching signal is emitted by the Message Bus the
-- handler function will be called as a result. The signature of the handler
-- function should match the signature specified by the D-Bus XML signal
-- description for the Message Bus. 
--
-- @tparam userdata ctrl Message Bus controller instance. 
-- @tparam string sigName The name of the Message Bus signal to receive.
-- @tparam func handler The signal handler function which is called with the
-- same arguments as specified for the signal in the D-Bus XML description
-- for the *org.freedesktop.DBus* interface. 
-- @treturn ?lightuserdata|nil Returns an opaque handle that can be used to
-- @{disconnectSignal|disconnect} the handler. Can be **nil** if there was an
-- error connecting to the signal.
-- @within MsgBusController
-- @function connectSignal
function MsgBusController:connectSignal(sigName, handler)
	return self.ctrl:connectSignal(l2dbus.Dbus.INTERFACE_DBUS, sigName, handler)
end


--- Disconnects a handler from a specific Message Bus signal.
-- 
-- This method disconnects the signal handler with the specified handle
-- returned by @{connectSignal}. 
--
-- @tparam userdata ctrl Message Bus controller instance. 
-- @tparam lightuserdata hnd The opaque handle originally returned by the call
-- to @{connectSignal}. 
-- @treturn bool Returns **true** if the handler was disconnected successfully
-- otherwise **false**.
-- @within MsgBusController
-- @function disconnectSignal
function MsgBusController:disconnectSignal(hnd)	
	return self.ctrl:disconnnectSignal(hnd)
end


--- Disconnects from **all** signals of the Message Bus.
-- 
-- This method makes a best effort to disconnects all the signal handlers for
-- all Message Bus signals. 
--
-- @tparam userdata ctrl Message Bus controller instance.
-- @within MsgBusController
-- @function disconnectAllSignals
function MsgBusController:disconnectAllSignals()
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
if require('l2dbus.is_main')() then
    -- The module is being run as a program
    main(arg)
else
    -- The module is being loaded rather than run
    return M
end
