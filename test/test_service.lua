#!/usr/bin/env lua

local state = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")

local disp = nil
local AUDIO_CAPTURE_OBJECT = "/org/example/service/AudioCapture";
local AUDIO_CAPTURE_INTERFACE_NAME = "org.example.AudioCapture"
local AUDIO_CAPTURE_BUS_NAME = "org.example.service.AudioCapture"
local AUDIO_CAPTURE_METHODS =
{
	{
	name = "openSession",
	args =
		{
			{name = "busName", sig="s", dir="in"},
			{name = "objectPath", sig="s", dir="in"},
			{name = "audioMgtClient", sig="b", dir="in"},
			{name = "sessionId", sig="u", dir="out"}
		}
	},
	{
	name = "closeSession",
	args =
		{
			{name = "sessionId", sig="u", dir="in"},
			{sig="a{sv}", dir="out"}
		}
	},
	{
	name = "getSources",
	args =
		{
			{name="sources", sig="as", dir="out"}
		}
	},
	{
	name = "getCodecs",
	args =
		{
			{name="codecs", sig="as", dir="out"},
			{name="source", sig="s", dir="out"}
		}
	},
	{
	name = "startCapture",
	args =
		{
			{name="sessionId", sig="u", dir="in"},
			{name="hostname", sig="s", dir="in"},
			{name="port", sig="q", dir="in"},
			{name="codec", sig="s", dir="in"},
			{name="source", sig="s", dir="in"},
			{sig="a{sv}", dir="out"}
		}
	},
	{
	name = "stopCapture",
	args =
		{
			{name="sessionId", sig="u", dir="in"},
			{sig="a{sv}", dir="out"}
		}
	},
	{
	name = "quit",
	},
}

local AUDIO_CAPTURE_SIGNALS =
{
	{
	name = "speechChange",
	args =
		{
			{name = "sessionId", sig="u"},
			{name = "isActive", sig="b"}
		}
	},
	{
	name = "audioLevelChange",
	args =
		{
			{name = "sessionId", sig="u"},
			{name = "peakLevel", sig="d"}
		}
	}
}

local AUDIO_CAPTURE_PROPS =
{
	{
	name = "isActive",
	sig="b",
	access="r"
	},
	{
	name = "peakLevel",
	sig="d",
	access="r"
	}
}


local AUDIO_PLAYER_OBJECT = "/org/example/service/AudioPlayer";
local AUDIO_PLAYER_INTERFACE_NAME = "org.example.service.AudioPlayer"

local function dumpMessage(msg)
	-- Dump out the message
	local msgType = msg:getType()
	io.stdout:write(l2dbus.Message.msgTypeToString(msgType) .. " sender=" ..
					tostring(msg:getSender()) .. " -> dest=" ..
					tostring(msg:getDestination()) .. " serial=" ..
					tostring(msg:getSerial()))
	if (l2dbus.Message.METHOD_CALL == msgType) or
		(l2dbus.Message.SIGNAL == msgType) then
		io.stdout:write(" path=" .. tostring(msg:getObjectPath()) ..
					" interface=" .. tostring(msg:getInterface()) ..
					" member=" .. tostring(msg:getMember()))
	elseif l2dbus.Message.ERROR == msgType then
		io.stdout:write(" name=" .. tostring(msg:getErrorName()))
	end

	local result = msg:getArgsAsArray()
	print()
	for i=1,#result do
		if type(result[i]) == "table" then
			io.stdout:write("table: " .. pretty.write(result[i], "  ") .. "\n")
		else
			print("  " .. type(result[i]) .. ": " .. tostring(result[i]))
		end
	end
end


local function onAudioCaptureRequest(intf, conn, msg, userdata)
	print("onAudioCaptureRequest: " .. msg:getObjectPath() ..
	      " serial # = " .. msg:getSerial())
	dumpMessage(msg)
	local result = l2dbus.Dbus.HANDLER_RESULT_NOT_YET_HANDLED
	local member = msg:getMember()
	if "quit" == member then
		local reply = l2dbus.Message.newMethodReturn(msg)
		conn:send(reply)
		disp:stop()
		result = l2dbus.Dbus.HANDLER_RESULT_HANDLED
	elseif ("closeSession" == member) or ("getCodecs" == member) or
	   ("getSources" == member) or ("openSession" == member) or
	   ("startCapture" == member) or ("stopCapture") then
	   local reply = l2dbus.Message.newError(msg,
	                   l2dbus.Dbus.ERROR_NOT_SUPPORTED,
	                   string.format("Member <%s> not implemented", member))
	   conn:send(reply)
	   result = l2dbus.Dbus.HANDLER_RESULT_HANDLED
	end

	return result;
end


local function defaultServiceHandler(svcObj, conn, msg, userdata)
	print("DefaultServiceHandler: " .. svcObj:path())
	dumpMessage(msg)
	return l2dbus.HANDLER_RESULT_NOT_YET_HANDLED;
end

local function main()
	local result = nil
	l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)

	local mainLoop
	if (arg[1] == "--glib") or (arg[1] == "-g") then
		mainLoop = require("l2dbus_glib").MainLoop.new()
	else
		mainLoop = require("l2dbus_ev").MainLoop.new()
	end
	
    disp = l2dbus.Dispatcher.new(mainLoop)
    assert( nil ~= disp )
    local conn = l2dbus.Connection.openStandard(disp, l2dbus.Dbus.BUS_SESSION)
    assert( nil ~= conn )

    local msg = l2dbus.Message.newMethodCall({destination=l2dbus.Dbus.SERVICE_DBUS,
				path=l2dbus.Dbus.PATH_DBUS, interface=l2dbus.Dbus.INTERFACE_DBUS,
				method="RequestName"})
	msg:addArgsBySignature("su", AUDIO_CAPTURE_BUS_NAME, l2dbus.Dbus.NAME_FLAG_DO_NOT_QUEUE)

	print("Requesting a bus name: " .. AUDIO_CAPTURE_BUS_NAME)
	local reply, errName, errMsg = conn:sendWithReplyAndBlock(msg)
	if reply == nil then
		error("Request Failed =>  " .. tostring(errName) .. " : " .. tostring(errMsg))
	else
		result = reply:getArgs()
		assert( result == l2dbus.Dbus.REQUEST_NAME_REPLY_PRIMARY_OWNER )
		print("Acquired name successfully")
	end

	local svcObj = l2dbus.ServiceObject.new(AUDIO_CAPTURE_OBJECT,
											defaultServiceHandler,
											"DefaultHandler")
	local acsInf = l2dbus.Interface.new(AUDIO_CAPTURE_INTERFACE_NAME,
										onAudioCaptureRequest,
										"AudioCaptureHandler")
	acsInf:registerMethods(AUDIO_CAPTURE_METHODS)
	acsInf:registerSignals(AUDIO_CAPTURE_SIGNALS)
	acsInf:registerProperties(AUDIO_CAPTURE_PROPS)
	acsInf:clearMethods()
	acsInf:clearSignals()
	acsInf:clearProperties()
	acsInf:registerMethods(AUDIO_CAPTURE_METHODS)
	acsInf:registerSignals(AUDIO_CAPTURE_SIGNALS)
	acsInf:registerProperties(AUDIO_CAPTURE_PROPS)
	local introspectIf = l2dbus.Introspection.new()

	svcObj:addInterface(acsInf)
	svcObj:removeInterface(acsInf)
	svcObj:addInterface(acsInf)

	svcObj:addInterface(introspectIf)
	svcObj:removeInterface(introspectIf)
	svcObj:addInterface(introspectIf)

	assert( conn:registerServiceObject(svcObj) )
	assert( conn:unregisterServiceObject(svcObj) )
	assert( conn:registerServiceObject(svcObj) )

    print("Starting main loop")
    disp:run(l2dbus.Dispatcher.DISPATCH_WAIT)

    -- Free all resources
    assert( conn:unregisterServiceObject(svcObj) )
    svcObj:removeInterface(acsInf)
    svcObj:removeInterface(introspectIf)
    acsInf = nil
    introspectIf = nil
    svcObj = nil
    msg = nil
    conn = nil
    disp = nil
end


print("Hit Return to continue")
io.stdin:read("*l")
print("Starting program")
main()

l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
state.dump_stats(io.stdout)
