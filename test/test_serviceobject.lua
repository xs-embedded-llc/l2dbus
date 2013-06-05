#!/usr/bin/env lua

local state = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")
local proxyctrl = require("l2dbus.proxyctrl")
local svc = require("l2dbus.service")

--
-- Module-scoped globals
--
local gDispatcher = nil
local gService = nil
local gTestProps = {rProp =1, wProp=2, rwProp=3}

--
-- Constants
--
local L2DBUS_TEST_OBJECT = "/org/l2dbus/service/Object";
local L2DBUS_TEST_INTERFACE_NAME = "org.l2dbus.Interface"
local L2DBUS_TEST_BUS_NAME = "org.l2dbus.service.Test"
local L2DBUS_TEST_ERROR_NAME = "org.l2dbus.Error"
local L2DBUS_TEST_WIRE_METADATA =
[[
<!DOCTYPE node PUBLIC "-//freedesktop//DTD D-BUS Object Introspection 1.0//EN"
"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd">
<node>
  <interface name="org.l2dbus.Interface">
    <method name="quit" />
    <method name="SendRecvIntegralTypes">
      <arg direction="in" name="aByte" type="y"/>
      <arg direction="in" name="aBoolean" type="b"/>
      <arg direction="in" name="aInt16" type="n"/>
      <arg direction="in" name="aUint16" type="q"/>
      <arg direction="in" name="aInt32" type="i"/>
      <arg direction="in" name="aUint32" type="u"/>
      <arg direction="in" name="aInt64" type="x"/>
      <arg direction="in" name="aUint64" type="t"/>
      <arg direction="in" name="aDouble" type="d"/>
      <arg direction="out" name="aByte" type="y"/>
      <arg direction="out" name="aBoolean" type="b"/>
      <arg direction="out" name="aInt16" type="n"/>
      <arg direction="out" name="aUint16" type="q"/>
      <arg direction="out" name="aInt32" type="i"/>
      <arg direction="out" name="aUint32" type="u"/>
      <arg direction="out" name="aInt64" type="x"/>
      <arg direction="out" name="aUint64" type="t"/>
      <arg direction="out" name="aDouble" type="d"/>
    </method>
    <method name="SendRecvStringTypes">
      <arg direction="in" name="aString" type="s"/>
      <arg direction="in" name="aObjPath" type="o"/>
      <arg direction="in" name="aSignature" type="g"/>
      <arg direction="out" name="aString" type="s"/>
      <arg direction="out" name="aObjPath" type="o"/>
      <arg direction="out" name="aSignature" type="g"/>
    </method>
    <method name="SendRecvSimpleArray">
      <arg direction="in" name="aInt32Array" type="ai"/>
      <arg direction="in" name="aStringArray" type="as"/>
      <arg direction="in" name="aInt64Array" type="ax"/>
      <arg direction="out" name="aInt32Array" type="ai"/>
      <arg direction="out" name="aStringArray" type="as"/>
      <arg direction="out" name="aInt64Array" type="ax"/>
    </method>
    <method name="SendRecvComplexArray">
      <arg direction="in" name="anArrayofArrayInt32" type="aai"/>
      <arg direction="in" name="aStructArray" type="a(isas)"/>
      <arg direction="in" name="aHash" type="a{sv}"/>
      <arg direction="out" name="anArrayofArrayInt32" type="aai"/>
      <arg direction="out" name="aStructArray" type="a(isas)"/>
      <arg direction="out" name="aHash" type="a{sv}"/>
    </method>
    <method name="SendRecvComplexStruct">
      <arg direction="in" type="(dasa{sv})"/>
      <arg direction="out" type="(dasa{sv})"/>
    </method>
    <method name="SendDelayedResponse">
      <arg direction="in" type="i"/>
      <arg direction="out" type="i"/>
    </method>
    <signal name="NotifyIntegralTypes">
      <arg name="aByte" type="y"/>
      <arg name="aBoolean" type="b"/>
      <arg name="aInt16" type="n"/>
      <arg name="aUint16" type="q"/>
      <arg name="aInt32" type="i"/>
      <arg name="aUint32" type="u"/>
      <arg name="aInt64" type="x"/>
      <arg name="aUint64" type="t"/>
      <arg name="aDouble" type="d"/>
    </signal>
    <signal name="NotifyStringTypes">
      <arg name="aString" type="s"/>
      <arg name="aObjPath" type="o"/>
      <arg name="aSignature" type="g"/>
    </signal>
    <signal name="NotifyComplexStruct">
      <arg direction="out" type="(dasa{sv})"/>
    </signal>
    <property name="rProp" type="i" access="read" />
    <property name="wProp" type="i" access="write" />
    <property name="rwProp" type="i" access="readwrite" />
  </interface>
</node>
]]


--
-- Signal Handlers for D-Bus Proxy
--

local function onNameOwnerChanged(busName, oldOwner, newOwner)
	print("\nonNameOwnerChanged:")
	print("\tBusName: " .. busName)
	print("\tOldOwner: " .. oldOwner)
	print("\tNewOwner: " .. newOwner)
end


local function onNameLost(name)
	print("\nonNameLost: " .. name)
end


local function onNameAcquired(name)
	print("\nonNameAcquired: " .. name)
end


--
-- Service handlers
--

local function defaultSvcHandler(ctx, intfName, member, args)
	if member == "quit" then
		ctx:reply()
		ctx:getConnection():flush()
		gDispatcher:stop()
	else
		ctx:error(L2DBUS_TEST_ERROR_NAME, "unhandled request for " ..
				((intfName == nil) and "" or (intfName .. ".")) .. member)
	end
end


local function onSendRecvIntegralTypes
	(
	ctx,
	byteVal,
	boolVal,
	int16Val,
	uint16Val,
	int32Val,
	uint32Val,
	int64Val,
	uint64Val,
	doubleVal
	)

	ctx:reply(byteVal, boolVal, int16Val, uint16Val,
				int32Val, uint32Val, int64Val, uint64Val, doubleVal)
end


local function onSendRecvStringTypes
	(
	ctx,
	stringVal,
	objPathVal,
	sigVal
	)

	ctx:reply(stringVal, objPathVal, sigVal)
end


local function onSendRecvSimpleArray(ctx, a1, a2, a3)
	ctx:reply(a1, a2, a3)
end


local function onSendRecvComplexArray(ctx, a1, a2, a3)
	ctx:reply(a1, a2, a3)
end


local  function onSendRecvComplexStruct(ctx, s)
	ctx:reply(s)
end


local function onSendDelayedResponse(ctx, value)
	local function onReplyLater(tmout, data)
		data.ctx:reply(data.value)
	end

	local timeout = l2dbus.Timeout.new(gDispatcher, value, false,
										onReplyLater, {ctx=ctx, value=value})
    timeout:setEnable(true)
end


local function onPropertyGet(ctx, intfName, propName)
	if intfName and (intfName ~= L2DBUS_TEST_INTERFACE_NAME) then
		ctx:error(L2DBUS_TEST_ERROR_NAME, "unknown interface: " .. intfName)
	else
		local value = nil
		if (propName == "rProp") or (propName == "rwProp") then
			value = gTestProps[propName]
		end

		if value ~= nil then
			ctx:reply(value)
		else
			ctx:error(L2DBUS_TEST_ERROR_NAME, "unknown property: " .. propName)
		end
	end
end


local function onPropertySet(ctx, intfName, propName, value)
	if intfName and (intfName ~= L2DBUS_TEST_INTERFACE_NAME) then
		ctx:error(L2DBUS_TEST_ERROR_NAME, "unknown interface: " .. intfName)
	else
		if (propName == "wProp") or (propName == "rwProp") then
			if gTestProps[propName] ~= value then
				gTestProps[propName] = value
				gService:emit(ctx:getConnection(),
							l2dbus.Dbus.INTERFACE_PROPERTIES,
							"PropertiesChanged",
							L2DBUS_TEST_INTERFACE_NAME,
							{propName = value}, {})
			end
		end
		ctx:reply()
	end
end


local function onPropertyGetAll(ctx, intfName)
	if intfName and (intfName ~= L2DBUS_TEST_INTERFACE_NAME) then
		ctx:error(L2DBUS_TEST_ERROR_NAME, "unknown interface: " .. intfName)
	else
		ctx:reply(gTestProps)
	end
end


local function onEmitSignal(timeout, ctx)
	local sigName
	local value
	if ctx.idx == 1 then
		sigName = "NotifyIntegralTypes"
		value = {1, true, 2, 3, 4, 5, 6, 7, 8.8}
	elseif ctx.idx == 2 then
		sigName = "NotifyStringTypes"
		value = {"aString", "/org/obj/path", "a{sv}"}
	elseif ctx.idx == 3 then
		sigName = "NotifyComplexStruct"
		value = {{999.9,{"test1", "test2"},{large=10, name="bob", peak=666.6}}}
	end

	gService:emit(ctx.conn, L2DBUS_TEST_INTERFACE_NAME, sigName, unpack(value))
	ctx.idx = ctx.idx + 1
	if ctx.idx > 3 then
		ctx.idx = 1
	end
end



local function main()
	l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)
	local result

    gDispatcher = l2dbus.Dispatcher.new()
    assert( nil ~= gDispatcher )
    local conn = l2dbus.Connection.openStandard(gDispatcher, l2dbus.Dbus.BUS_SESSION)
    assert( nil ~= conn )

    local dbusProxyCtrl = proxyctrl.new(conn, l2dbus.Dbus.SERVICE_DBUS, l2dbus.Dbus.PATH_DBUS)
    dbusProxyCtrl:bind(true)
    dbusProxyCtrl:setBlockingMode(true)

    -- Register for the D-Bus signals
    assert(dbusProxyCtrl:connectSignal(l2dbus.Dbus.INTERFACE_DBUS,
									   "NameOwnerChanged",
									   onNameOwnerChanged))
    assert(dbusProxyCtrl:connectSignal(l2dbus.Dbus.INTERFACE_DBUS,
									   "NameLost",
									   onNameLost))
    assert(dbusProxyCtrl:connectSignal(l2dbus.Dbus.INTERFACE_DBUS,
									   "NameAcquired",
									   onNameAcquired))

	local  dbusProxy = dbusProxyCtrl:getProxy(l2dbus.Dbus.INTERFACE_DBUS)

    _,result = dbusProxy.m.ListNames()
    print("The current registered Bus Names:")
    pretty.dump(result)

	_,result = dbusProxy.m.NameHasOwner(L2DBUS_TEST_BUS_NAME)
    if result then
    	error("Bus name " .. L2DBUS_TEST_BUS_NAME .. " is already claimed!")
    end

    _,result = dbusProxy.m.RequestName(L2DBUS_TEST_BUS_NAME, l2dbus.Dbus.NAME_FLAG_DO_NOT_QUEUE)
    assert( result == l2dbus.Dbus.REQUEST_NAME_REPLY_PRIMARY_OWNER )


    -- Register the interface
    gService = svc.new(L2DBUS_TEST_OBJECT, true, defaultSvcHandler)
    local metadata = svc.convertXmlToIntfMeta(L2DBUS_TEST_INTERFACE_NAME,
    											  L2DBUS_TEST_WIRE_METADATA)
    assert( gService:addInterface(L2DBUS_TEST_INTERFACE_NAME, metadata) )

    gService:registerMethodHandler(L2DBUS_TEST_INTERFACE_NAME,
    										"SendRecvIntegralTypes",
    										onSendRecvIntegralTypes)

    gService:registerMethodHandler(L2DBUS_TEST_INTERFACE_NAME,
    										"SendRecvStringTypes",
    										onSendRecvStringTypes)

    gService:registerMethodHandler(L2DBUS_TEST_INTERFACE_NAME,
    										"SendRecvSimpleArray",
    										onSendRecvSimpleArray)

    gService:registerMethodHandler(L2DBUS_TEST_INTERFACE_NAME,
    										"SendRecvComplexArray",
    										onSendRecvComplexArray)

    gService:registerMethodHandler(L2DBUS_TEST_INTERFACE_NAME,
    										"SendRecvComplexStruct",
    										onSendRecvComplexStruct)

    gService:registerMethodHandler(L2DBUS_TEST_INTERFACE_NAME,
    										"SendDelayedResponse",
    										onSendDelayedResponse)

    gService:registerMethodHandler(l2dbus.Dbus.INTERFACE_PROPERTIES,
    										"Set",
    										onPropertySet)

    gService:registerMethodHandler(l2dbus.Dbus.INTERFACE_PROPERTIES,
    										"Get",
    										onPropertyGet)

    gService:registerMethodHandler(l2dbus.Dbus.INTERFACE_PROPERTIES,
    										"GetAll",
    										onPropertyGetAll)


	local timeout = l2dbus.Timeout.new(gDispatcher, 4000, true,
										onEmitSignal, {conn=conn, idx = 1})
	timeout:setEnable(true)

    -- Bind the service object to the connection
    gService:attach(conn)

    print("Starting main loop")
    gDispatcher:run(l2dbus.Dispatcher.DISPATCH_WAIT)

    gService:detach(conn)
    dbusProxyCtrl:setBlockingMode(true)
    dbusProxyCtrl:disconnectAllSignals()
    assert(l2dbus.Dbus.RELEASE_NAME_REPLY_RELEASED ==
    	dbusProxy.m.ReleaseName(L2DBUS_TEST_BUS_NAME))
    conn:flush()
    timeout = nil
    gService = nil
    conn = nil
    gDispatcher = nil
end


print("Hit Return to continue")
--io.stdin:read("*l")
print("Starting program")
main()

l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
state.dump_stats(io.stdout)