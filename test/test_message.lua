#!/usr/bin/env lua

local state = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")

local function dumpMsg(msg)
	local buf = msg:marshallToArray()
	for idx = 1,#buf do
		local value = buf[idx]
		io.stdout:write(string.format("%02x", value))
		if (value >= 32) and (value <= 127) then
			io.stdout:write(string.format("[%c] ", value))
		else
			io.stdout:write(" ")
		end
		io.stdout:write()
	end
	io.stdout:write("\n")
end

local function pack(...)
	return { n = select("#", ...), ... }
end

local function testMsgSetArgs(...)
	local msg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	msg:addArgs(...)
	print("Msg signature is: " .. msg:getSignature())
	print("Signature is: " .. (l2dbus.Message.validateSignature(msg:getSignature())
			and "valid" or "invalid"))
	print("Msg contents: ", dumpMsg(msg))
	local result = pack(msg:getArgs())
	print("Demarshalled args: " .. pretty.write(result))
	msg = nil
end

local function testMsgSetArgsBySig(signature, ...)
	local msg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	msg:addArgsBySignature(signature, ...)
	print("Msg signature is: " .. msg:getSignature())
	print("Signature is: " .. (l2dbus.Message.validateSignature(msg:getSignature())
			and "valid" or "invalid"))
	print("Msg contents: ", dumpMsg(msg))
	local result = pack(msg:getArgs())
	print("Demarshalled args: " .. pretty.write(result))
	msg = nil
end


local function main()
    print("Dumping l2dbus_core")
    pretty.dump(l2dbus)

    pretty.dump(l2dbus.Trace.getFlags())
    l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)
	local dbt = l2dbus.DbusTypes

	local minByte = dbt.Byte.new(0)
	print("minByte: dbusTypeId " .. minByte:dbusTypeId() .. " value = " .. minByte:value())
	local maxByte = dbt.Byte.new(255)
	print("maxByte: dbusTypeId " .. maxByte:dbusTypeId() .. " value = " .. maxByte:value())
	local minInt16 = dbt.Int16.new(-32768)
	print("minInt16: dbusTypeId " .. minInt16:dbusTypeId() .. " value = " .. minInt16:value())
	local maxInt16 = dbt.Int16.new(32767)
	print("maxInt16: dbusTypeId " .. maxInt16:dbusTypeId() .. " value = " .. maxInt16:value())
	local minUint16 = dbt.Uint16.new(0)
	print("minUint16: dbusTypeId " .. minUint16:dbusTypeId() .. " value = " .. minUint16:value())
	local maxUint16 = dbt.Uint16.new(65535)
	print("maxUint16: dbusTypeId " .. maxUint16:dbusTypeId() .. " value = " .. maxUint16:value())
	local minInt32 = dbt.Int32.new(-2147483648)
	print("minInt32: dbusTypeId " .. minInt32:dbusTypeId() .. " value = " .. minInt32:value())
	local maxInt32 = dbt.Int32.new(2147483647)
	print("maxInt32: dbusTypeId " .. maxInt32:dbusTypeId() .. " value = " .. maxInt32:value())
	local minUint32 = dbt.Uint32.new(0)
	print("minUint32: dbusTypeId " .. minUint32:dbusTypeId() .. " value = " .. minUint32:value())
	local maxUint32 = dbt.Uint32.new(4294967295)
	print("maxUint32: dbusTypeId " .. maxUint32:dbusTypeId() .. " value = " .. maxUint32:value())
	local minInt64 = dbt.Int64.new(l2dbus.Int64.new("-9223372036854775808"))
	print("minInt64: dbusTypeId " .. minInt64:dbusTypeId() .. " value = " .. minInt64:value())
	local maxInt64 = dbt.Int64.new(l2dbus.Int64.new("9223372036854775807"))
	print("maxInt64: dbusTypeId " .. maxInt64:dbusTypeId() .. " value = " .. maxInt64:value())
	local minUint64 = dbt.Uint64.new(l2dbus.Uint64.new("0"))
	print("minUint64: dbusTypeId " .. minUint64:dbusTypeId() .. " value = " .. minUint64:value())
	local maxUint64 = dbt.Uint64.new(l2dbus.Uint64.new("18446744073709551615"))
	print("maxUint64: dbusTypeId " .. maxUint64:dbusTypeId() .. " value = " .. maxUint64:value())
	local maxDouble = dbt.Double.new(1.7976931348623158e+308)
	print("maxDouble: dbusTypeId " .. maxDouble:dbusTypeId() .. " value = " .. maxDouble:value())
	local minDouble = dbt.Double.new(2.2250738585072014e-308)
	print("minDouble: dbusTypeId " .. minDouble:dbusTypeId() .. " value = " .. minDouble:value())
	local strValue = dbt.String.new("This is a test string")
	print("strValue: dbusTypeId " .. strValue:dbusTypeId() .. " value = " .. strValue:value())
	local sigValue = dbt.Signature.new("a(ii)")
	print("sigValue: dbusTypeId " .. sigValue:dbusTypeId() .. " value = " .. sigValue:value())
	local objPathValue = dbt.ObjectPath.new("/com/acme")
	print("objPathValue: dbusTypeId " .. objPathValue:dbusTypeId() .. " value = " .. objPathValue:value())
	local unixFdValue = dbt.UnixFd.new(5)
	print("unixFdValue: dbusTypeId " .. unixFdValue:dbusTypeId() .. " value = " .. unixFdValue:value())
	local dictValue = dbt.Dictionary.new({a = 4, b = "ball"})
	print("dictValue: dbusTypeId " .. dictValue:dbusTypeId() .. " value = " .. pretty.write(dictValue:value()))
	local varValue = dbt.Variant.new(dictValue)
	print("varValue: dbusTypeId " .. varValue:dbusTypeId() .. " value = " .. pretty.write(varValue:value()))
	local structValue = dbt.Structure.new({[1] = "foo", [2]=666})
	print("structValue: dbusTypeId " .. structValue:dbusTypeId() .. " value = " .. pretty.write(structValue:value()))
	local arrayValue = dbt.Array.new({1,2,3,4,5})
	print("arrayValue: dbusTypeId " .. arrayValue:dbusTypeId() .. " value = " .. pretty.write(arrayValue:value()))
	local boolValue = dbt.Boolean.new(true)
	print("boolValue: dbusTypeId " .. boolValue:dbusTypeId() .. " value = " .. tostring(boolValue:value()))


	local res, val = pcall(dbt.Array.new, {[1]=l2dbus.Int64.new(1), [2]=l2dbus.Int64.new(2), [3]=l2dbus.Int64.new(3)})
	io.stdout:write("Creating 'good' array - ")
	if res then
		print("PASS")
	else
		print("FAIL: " .. val)
	end

	res, val = pcall(dbt.Array.new, {[1]=l2dbus.Int64.new(1), [2]='a', [3]='g'})
	io.stdout:write("Creating 'bad' array - ")
	if res == false then
		print("PASS")
	else
		print("FAIL: " ..  val)
	end

	res, val = pcall(dbt.Structure.new, {[1]=l2dbus.Int64.new(1), [2]=l2dbus.Int64.new(2),
							[3]=l2dbus.Int64.new(3), [4]=l2dbus.Int64.new(4)})
	io.stdout:write("Creating 'good' structure - ")
	if res then
		print("PASS")
	else
		print("FAIL: " .. val)
	end

	res, val = pcall(dbt.Structure.new, {[1]=l2dbus.Int64.new(1), [2]=l2dbus.Int64.new(2),
							[5]=l2dbus.Int64.new(3), a=l2dbus.Int64.new(4)})
	io.stdout:write("Creating 'bad' structure - ")
	if res == false then
		print("PASS")
	else
		print("FAIL: " .. val)
	end

	res, val = pcall(dbt.Dictionary.new, {x=l2dbus.Int64.new(1), y="george",
							z=4, ["blah"]=l2dbus.Int64.new(4)})
	io.stdout:write("Creating 'good' dictionary - ")
	if res then
		print("PASS")
	else
		print("FAIL: " .. val)
	end

	res, val = pcall(dbt.Dictionary.new, {x=l2dbus.Int64.new(1), [{g=4}]="george",
							z=4, ["blah"]=l2dbus.Int64.new(4)})
	io.stdout:write("Creating 'bad' dictionary - ")
	if res == false then
		print("PASS")
	else
		print("FAIL")
	end

	print("==========================================")
	print("         D-Bus Message Tests")
	print("==========================================")
	local dbusMsg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	assert( l2dbus.Message.METHOD_CALL == dbusMsg:getType() )
	dbusMsg = l2dbus.Message.new(l2dbus.Message.ERROR)
	assert( l2dbus.Message.ERROR == dbusMsg:getType() )
	dbusMsg = l2dbus.Message.new(l2dbus.Message.SIGNAL)
	assert( l2dbus.Message.SIGNAL == dbusMsg:getType() )
	dbusMsg = l2dbus.Message.new(l2dbus.Message.METHOD_RETURN)
	assert( l2dbus.Message.METHOD_RETURN == dbusMsg:getType() )
	res,val = pcall(l2dbus.Message.new, l2dbus.Message.INVALID)
	if res == false then
		print("PASS - attempt to create invalid message")
	else
		print("FAIL: " .. val)
	end

	dbusMsg = l2dbus.Message.newMethodCall(nil, "/com/acme", nil, "foo")
	dbusMsg = l2dbus.Message.newMethodCall({destination=nil, path="/com/acme", interface=nil, method="foo"})
	res,val = pcall(l2dbus.Message.newMethodCall, "com.acme", nil, nil, "foo")
	if res == false then
		print("PASS - attempt to create invalid call message")
	else
		print("FAIL: " .. val)
	end

	-- A reply message must have a serial # > 0
	dbusMsg:setSerial(5)
	dbusMsg = l2dbus.Message.newMethodReturn(dbusMsg)

	res,val = pcall(l2dbus.Message.newMethodReturn, nil)
	if res == false then
		print("PASS - attempt to create invalid return message")
	else
		print("FAIL: " .. val)
	end

	dbusMsg = l2dbus.Message.newSignal("/com/acme", "com.acme", "sigName")
	dbusMsg = l2dbus.Message.newSignal({path="/com/acme", interface="com.acme", name="sigName"})
	res,val = pcall(l2dbus.Message.newSignal, "/com/acme", nil, "sigName")
	if res == false then
		print("PASS - attempt to create invalid signal message")
	else
		print("FAIL: " .. val)
	end

	dbusMsg = l2dbus.Message.newMethodCall(nil, "/com/acme", nil, "foo")
	dbusMsg:setSerial(5)
	dbusMsg = l2dbus.Message.newError(dbusMsg, "com.acme.error", "error message")
	res,val = pcall(l2dbus.Message.newError, nil, "com.acme.error", "error message")
	if res == false then
		print("PASS - attempt to create invalid error message")
	else
		print("FAIL: " .. val)
	end

	dbusMsg = l2dbus.Message.newSignal({path="/com/acme", interface="com.acme", name="sigName"})
	dbusMsg = l2dbus.Message.copy(dbusMsg)
	res,val = pcall(l2dbus.Message.copy, nil)
	if res == false then
		print("PASS - attempt to copy invalid message")
	else
		print("FAIL: " .. val)
	end


	dbusMsg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	print("Current => GetNoReply: " .. (dbusMsg:getNoReply() and "yes" or "no"))
	dbusMsg:setNoReply(true)
	print("Set to 'true' => GetNoReply: " .. (dbusMsg:getNoReply() and "yes" or "no"))
	dbusMsg:setNoReply(false)
	print("Set to 'false' => GetNoReply: " .. (dbusMsg:getNoReply() and "yes" or "no"))

	print("Current => GetAutoStart: " .. (dbusMsg:getAutoStart() and "yes" or "no"))
	dbusMsg:setAutoStart(true)
	print("Set to 'true' => GetAutoStart: " .. (dbusMsg:getAutoStart() and "yes" or "no"))
	dbusMsg:setAutoStart(false)
	print("Set to 'false' => GetAutoStart: " .. (dbusMsg:getAutoStart() and "yes" or "no"))

	dbusMsg:setObjectPath("/com/acme")
	print("Getting object path: " ..  dbusMsg:getObjectPath())
	print("Has object path /com/acme: " .. (dbusMsg:hasObjectPath("/com/acme") and "true" or "false"))
	print("Has object path /com/fido: " .. (dbusMsg:hasObjectPath("/com/fido") and "true" or "false"))
	dbusMsg:setObjectPath(nil)
	print("Clear object path: " .. tostring(dbusMsg:getObjectPath()))
	dbusMsg:setObjectPath("/com/acme/widget")
	local paths = dbusMsg:getDecomposedObjectPath()
	print("Decomposing path: " .. dbusMsg:getObjectPath())
	pretty.dump(paths)
	dbusMsg:setObjectPath(nil)
	paths = dbusMsg:getDecomposedObjectPath()
	print("Decomposing path: " .. tostring(dbusMsg:getObjectPath()))
	pretty.dump(paths)


	dbusMsg:setInterface("com.acme")
	print("Getting interface: " ..  dbusMsg:getInterface())
	print("Has interface com.acme: " .. (dbusMsg:hasInterface("com.acme") and "true" or "false"))
	print("Has interface com.fido: " .. (dbusMsg:hasInterface("com.fido") and "true" or "false"))
	dbusMsg:setInterface(nil)
	print("Clear interface: " .. tostring(dbusMsg:getInterface()))

	dbusMsg:setMember("foo")
	print("Getting member: " ..  dbusMsg:getMember())
	print("Has member foo: " .. (dbusMsg:hasMember("foo") and "true" or "false"))
	print("Has member bar: " .. (dbusMsg:hasMember("bar") and "true" or "false"))
	dbusMsg:setMember(nil)
	print("Clear member: " .. tostring(dbusMsg:getMember()))

	dbusMsg = l2dbus.Message.new(l2dbus.Message.ERROR)
	print("Getting error name: " ..  tostring(dbusMsg:getErrorName()))
	print("Setting error name: com.acme.error")
	dbusMsg:setErrorName("com.acme.error")
	print("Getting error name: " ..  dbusMsg:getErrorName())
	dbusMsg:setErrorName(nil)
	print("Setting error name to nil")
	print("Getting error name: " ..  tostring(dbusMsg:getErrorName()))


	dbusMsg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	dbusMsg:setDestination("com.acme")
	print("Getting destination: " ..  dbusMsg:getDestination())
	print("Has destination com.acme: " .. (dbusMsg:hasDestination("com.acme") and "true" or "false"))
	print("Has destination com.fido: " .. (dbusMsg:hasDestination("com.fido") and "true" or "false"))
	dbusMsg:setDestination(nil)
	print("Clear destination: " .. tostring(dbusMsg:getDestination()))

	dbusMsg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	dbusMsg:setSender("com.acme.widget")
	print("Getting sender: " ..  dbusMsg:getSender())
	print("Has sender com.acme.widget: " .. (dbusMsg:hasSender("com.acme.widget") and "true" or "false"))
	print("Has sender com.fido.widget: " .. (dbusMsg:hasSender("com.acme.fido") and "true" or "false"))
	dbusMsg:setSender(nil)
	print("Clear sender: " .. tostring(dbusMsg:getSender()))

	print("Current message signature: " .. tostring(dbusMsg:getSignature()))
	dbusMsg:setSerial(666)
	print("Current serial # should be 666 = > " .. tostring(dbusMsg:getSerial()))

	print("=======================================")
	print("   Testing Marshalling/Unmarshalling   ")
	print("=======================================")


	dbusMsg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	dbusMsg:addArgs(maxByte, maxUint16, maxInt16, maxUint32, maxInt32,
					maxUint64, maxInt64, maxDouble, strValue, sigValue,
					objPathValue, boolValue)
	print("The signature is: " .. dbusMsg:getSignature())
	dbusMsg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	dbusMsg:addArgs(255, 65535, 32767, 4294967295, 2147483647, dbt.Uint64.new(5),
					dbt.Int64.new(6), 10.5, "Hi", "aii", "/com/acme", true)
	print("The signature is: " .. dbusMsg:getSignature())

	dbusMsg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	dbusMsg:addArgs(2147483647, 2147483648, -2147483647, -2147483648,
					9007199254740991, 9007199254740992, -9007199254740991, -9007199254740992,
					9007199254740992, -9007199254741993, 9007199254741991.1)
	print("The signature for marshalled numbers is: " .. dbusMsg:getSignature())


	testMsgSetArgs({a="bob", b="glenn"}, dictValue, structValue, varValue, arrayValue)
	testMsgSetArgs(structValue)
	testMsgSetArgsBySig("a{sv}i(si)",{a=1, b="glenn", c={1,2,4}}, 5,{"bob", 666})
	testMsgSetArgsBySig("v", {a=1})
	testMsgSetArgsBySig("a(ss)",{{"bob",dbt.String.new("tom")}, {"mike","ernie"}})
	testMsgSetArgsBySig("a{ss}",{a="bob",b="tom"})
	testMsgSetArgsBySig("a{ss}",{})
	testMsgSetArgsBySig("a{sa(xi)}", {one={{64, 32}}, two={{128, 64}}})
	testMsgSetArgs(dbt.Variant.new({{a="bob", b="tom"}, {smart="dumb", happy="sad"}}))
	testMsgSetArgs(1,2,3)

	testMsgSetArgs(dbt.Array.new({1, 2, 3}, "ax"))
	testMsgSetArgs(dbt.Dictionary.new({a=5, b=6.666}, "a{si}"))
	testMsgSetArgs(dbt.Structure.new({"a", 787}, "(su)"))
	testMsgSetArgs(dbt.Variant.new({"a", 787, 66.6, dbt.Array.new({255.8, 42},"ay")}, "v(stiv)"))
	testMsgSetArgsBySig("v", {"a", dbt.Double.new(787)})

	dbusMsg = l2dbus.Message.new(l2dbus.Message.METHOD_CALL)
	dbusMsg:addArgsBySignature("y", maxByte)
	dbusMsg:addArgsBySignature("(su)", {"hi", 666})
	dbusMsg:addArgsBySignature("i", 55)
	print("The signature of packed-by-parts msg: " .. dbusMsg:getSignature())
	dumpMsg(dbusMsg)

	dbusMsg = nil

end


print("Hit Return to continue")
io.stdin:read("*l")
print("Starting program")
main()

l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
state.dump_stats(io.stdout)