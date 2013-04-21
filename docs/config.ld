-- ldoc configuration file
project = 'L2DBUS'
description = 'L2DBUS Lua Libraries 1.0.0'
title = 'Lua-To-D-Bus (L2DBUS) Documentation'
package = 'l2dbus'
dir = 'api'
topics = './manual'
format = 'markdown'
new_type("constant", "Constants")
new_type("messageType", "Message Types")
new_type("handlerReturn", "Message Handler Return Values")
new_type("dbusError", "D-Bus Errors")
new_type("namespace", "Namespaces", true)

file = { '../lua/l2dbus', '../src' }
