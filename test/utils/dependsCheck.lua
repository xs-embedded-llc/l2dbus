#!/usr/bin/env lua
---------------------------------------------------------------------------
-- Lua Dependency Check
--
-- This will verify the necessay Lua dependencies are loaded
-- an warn the user for missing files/components.  This ensures
-- the correct modules are available for the tests to run.
---------------------------------------------------------------------------

print("Lua Dependency Checking...")

local commonLuaFiles = {    -- 3rd Party
                            "cjson.safe",
                            "pl.pretty",

                            "bit",            -- test_watcher.lua
                            "posix",          -- test_watcher.lua

                            -- l2dbus
                            "l2dbus",        -- init.lua
                            "l2dbus.msgbusctrl",
                            "l2dbus.proxyctrl",
                            "l2dbus.service",
                            "l2dbus.validate",
                            "l2dbus.xml",

                            --Utils/Helpers
                            "utils.prompter",
                            "utils.luastate",
                            "utils.traverse",
                        }

for _,name in ipairs(commonLuaFiles) do

    print("Checking: ", name)

    -- The require should be successful if the LUA_PATH is correct
    local bOk, sErr = pcall(require, name)
    if bOk == false then
        print("\n"..string.rep("*", 50))
        print(sErr)
        print("\n"..string.rep("*", 50))
        print("Fix the problem then try this script again\n\n")
        os.exit(1)
    end
end

print("Completed Successfully!\n")

