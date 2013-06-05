#!/usr/bin/env lua

local state = require("utils.luastate")
local pretty = require("pl.pretty")
local l2dbus = require("l2dbus")



local function main()
    local errMsg = nil

    print("Dumping l2dbus_core")
    pretty.dump(l2dbus)

    local a = l2dbus.Int64.new(1)
    print ("a = " .. a)
    local b = l2dbus.Int64.new("9223372036854775807", 10)
    print("Max Int64 = " .. b)
    local c = l2dbus.Int64.new(b - a)
    print(b .. " - " .. a .. " = " .. c)
    print("Convert number to octal = " .. c:toString(8))
    c = l2dbus.Int64.new("-9223372036854775808", 10);
    print("Min Int64 = " .. c)

    a = l2dbus.Int64.new(4)
    b = l2dbus.Int64.new(6)
    print(a .. " + " .. b .. " = " .. a+b)
    print(a .. " + 10 = " .. a+10)
    print("10 + " .. b .. " = " .. 10 + b)
    print(a .. " * " .. b .. " = " .. a*b)
    print(b .. " / " .. a .. " = " .. b / a)
    print("6.0 / " .. a .. " = " .. 6.0 / a)
    print("a = " .. a)
    print("-a = " .. -a)
    print(b .. " ^ " .. a .. " = " .. b ^ a)
    print(b .. " ^ " .. -a .. " = " .. b ^ (-a))
    print(b .. " ^ 0  = " .. b ^ 0)
    print(b .. " > " .. a .. " = " .. (b > a and "yes" or "no"))
    print(a .. " > " .. b .. " = " .. (a > b and "yes" or "no"))
    c = b
    print(c .. " == " .. b .. " = " .. (c == b and "yes" or "no"))
    print(a .. " == " .. b .. " = " .. (a == b and "yes" or "no"))
    print(c .. " <= " .. b .. " = " .. (c <= b and "yes" or "no"))
    print(c .. " > " .. b .. " = " .. (c > b and "yes" or "no"))
    print(c .. " >= " .. b .. " = " .. (c >= b and "yes" or "no"))
    print(b .. " % " .. a .. " = " .. b % a)
    print("#b = " .. #b)
    print("a:toNumber = " .. a:toNumber())

end


print("Hit Return to continue")
io.stdin:read("*l")
print("Starting program")
main()

l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
state.dump_stats(io.stdout)