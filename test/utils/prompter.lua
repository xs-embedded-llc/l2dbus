-------------------------------------------------------------------------------
--- prompter
---
--- This module is a flexible prompter object for getting user input on a
--- coroutine and not blocking other coroutines.  Common use is to get
--- user input for client apps while allowing DBus messages to be processed
--- while waiting for the user input.                                       <br>
---
--- @copyright   2013 XS-Embedded LLC
--- @license     MIT
---
--- @module prompter
-------------------------------------------------------------------------------

local l2dbus = require("l2dbus")
local ev     = require("ev")      -- for backward compatibility

local VERSION      = "2.0"

local prompterInst = nil
local Prompter     = {}
Prompter.__index   = Prompter

local g_cmdHist    = {}
local g_cmdHistMAX = 20

----------------------------------------------------------------
--- getInstance
---
--- Get an instance of the singleton prompter object.
---
--- @tparam   (object)  disp ....l2dbus dispatcher
---
--- @treturn  (object) prompter instance
----------------------------------------------------------------
function Prompter.getInstance(disp)
    if prompterInst == nil then
        local self = { initTermState = "",
                       pending       = nil,
                       stdIn         = io.stdin,
                       stdOut        = io.stdout,
                       stdInIo       = nil }

        prompterInst = setmetatable(self, Prompter)

        local stdInHandler = function()
            local stdIn = prompterInst.stdIn
            local ok, c = pcall(stdIn.read, stdIn, 1)
            if ok then
                if (type(prompterInst.pending) == "thread") and
                    (coroutine.status(prompterInst.pending) == "suspended") then
                    coroutine.resume(prompterInst.pending, c)
                end
            end
        end

        prompterInst.initTermState = self:stty()
        prompterInst.stdInIo       = l2dbus.Watch.new(disp, 0, "r", stdInHandler)
    end

    return prompterInst
end

----------------------------------------------------------------
--- new
---
--- (IMPORTANT: DEPRECATED, here for backward compatibility)
---
--- Create a new prompter object with its own command history.
---
--- @tparam   (object)  evLoop .... Lua ev loop
---
--- @treturn  (object) prompter instance
----------------------------------------------------------------
function Prompter:new(evLoop)
    if prompterInst == nil then
        local f = { initTermState = "",
                    pending       = nil,
                    loop          = evLoop,
                    stdIn         = io.stdin,
                    stdOut        = io.stdout,
                    stdInIo       = nil }

        prompterInst = setmetatable(f, self)

        local stdInHandler = function(loop, io, events)
            local stdIn = prompterInst.stdIn
            local ok, c = pcall(stdIn.read, stdIn, 1)
            if ok then
                if (type(prompterInst.pending) == "thread") and
                    (coroutine.status(prompterInst.pending) == "suspended") then
                    coroutine.resume(prompterInst.pending, c)
                end
            end
        end

        prompterInst.initTermState = self:stty()
        prompterInst.stdInIo       = ev.IO.new(stdInHandler, 0, ev.READ)
    end

    return prompterInst
end


----------------------------------------------------------------
--- addToCmdHist
---
--- Add a [string] command to the command history
---
--- @tparam   (string)  sCmd ....command to add
----------------------------------------------------------------
function Prompter:addToCmdHist(sCmd)
    -- Add only if different than the top entry
    if #g_cmdHist == 0 or g_cmdHist[#g_cmdHist] ~= sCmd then
        table.insert( g_cmdHist, sCmd )
        -- Allow a mx limit of commands in the history
        if #g_cmdHist > g_cmdHistMAX then
            table.remove( g_cmdHist, 0  )
        end
    end
end

----------------------------------------------------------------
--- stty
---
--- Handle stty calls
---
--- @tparam   (string)  ...  args to stty
---
--- @treturn  (table) state
----------------------------------------------------------------
function Prompter:stty(...)
    local ok, p = pcall(io.popen, "stty -g", "r")
    if not ok then return nil end
    local state = p:read("*a")
    p:close()

    local options = ""
    for i = 1, select('#', ...) do
        options = options .. " " .. tostring(select(i, ...))
    end

    if state then
        ok, p = pcall(io.popen, "stty" .. options, "r")
        if ok then
            p:read("*a")
            p:close()
        end
    end
    return state
end

----------------------------------------------------------------
--- getChar
---
--- Get a character but DO NOT block!
---
--- NOTE: This does NOT add to the command history
---
--- @tparam   (boolean)  disableEcho ....true=no chars echoed
---
--- @treturn  (string) char from user input
----------------------------------------------------------------
function Prompter:getChar(disableEcho)
    local thread = coroutine.running()
    assert(thread ~= nil, "Error: can only be called from a coroutine")
    self.pending = thread
    local oldState = nil
    if disableEcho then
        oldState = self:stty("-echo", "cbreak")
    else
        oldState = self:stty("cbreak")
    end
    if self.loop then
        self.stdInIo:start(self.loop)
    else
        self.stdInIo:setEnable(true)
    end
    local value = coroutine.yield()
    self:stty(oldState)
    return value
end

----------------------------------------------------------------
--- getLineBlocking
---
--- Get a line of text but blocks!
---
--- NOTE: This DOES add to the command history
---
--- @treturn  (string) line from user input
----------------------------------------------------------------
function Prompter:getLineBlocking()
    local oldState = self:stty(self.initTermState)
    if self.loop then
        self.stdInIo:stop(self.loop)
    else
        self.stdInIo:setEnable(false)
    end
    self.pending = nil
    local line = self.stdIn:read("*l")
    self:stty(oldState)

    self:addToCmdHist(line)

    return line
end

----------------------------------------------------------------
--- getLine
---
--- Get a line of text but DO NOT block!
---
--- This also supports the UP/DOWN arrow with a command history.
---
--- NOTE: This DOES add to the command history
---
--- @tparam   (boolean) bSkipCmdHist ....true=not added to cmd history
---
--- @treturn  (string) line from user input
----------------------------------------------------------------
function Prompter:getLine( bSkipCmdHist )

    local origBuf = ""
    local buf     = ""
    local ch      = ""
    local iCh     = ""
    local escLen  = 0
    local newOut  = nil
    local cmdPos  = #g_cmdHist+1

    ch = self:getChar()
    while ch ~= "\n" do

        newOut = nil
        escLen = 0
        iCh    = string.byte( ch )

        if iCh == 127 or iCh == 8 then
            -- Handle: Backspace and Ctrl-h
            escLen = 3
            buf    = string.sub( buf, 1, #buf-1 )
        elseif iCh == 27 then
            -- Escape sequence completed
            -- NOTE: Does not work with all emulations
            escLen = 3
            local ch2 = self.stdIn:read(1)
            local ch3 = ""
            if ch2 == "[" then
                ch3 = self.stdIn:read(1)
                escLen = escLen + 1
            end

            --print( "\nPRE ESC:", ch2, ch3, cmdPos, g_cmdHist[cmdPos], #buf )

            -- Handle the UP/DOWN arrow command history
            if #g_cmdHist > 0 and (ch3 == "A" or ch3 == "B") then
                if ch3 == "A" then -- UP ARROW
                    if cmdPos == #g_cmdHist+1 then
                        -- Add current buffer
                        origBuf = buf
                    end
                    cmdPos = cmdPos - 1
                    if cmdPos < 1 then
                        cmdPos = cmdPos + 1 -- no wrap
                    end
                else -- DOWN ARROW
                    if cmdPos ~= #g_cmdHist then
                        cmdPos = cmdPos + 1
                        if cmdPos > #g_cmdHist then
                            cmdPos = cmdPos - 1 -- no wrap
                            newOut    = origBuf
                        end
                    else
                        cmdPos = cmdPos + 1
                        newOut = origBuf
                    end
                end

                --print( "\nEND ESC:", cmdPos, g_cmdHist[cmdPos], #buf )

                escLen = escLen + #buf
                if newOut == nil then
                    buf    = g_cmdHist[cmdPos]
                    newOut = buf
                else
                    buf = newOut
                end
            end

        else
            buf = buf .. ch
        end

        if escLen > 0  then
            -- Remove escaped output
            io.stdout:write(string.rep(string.char(8), escLen ))
            io.stdout:write(string.rep(" ", escLen ))
            io.stdout:write(string.rep(string.char(8), escLen ))
        end

        if newOut then
            io.stdout:write(newOut)
        end

        ch  = self:getChar()
    end

    if bSkipCmdHist ~= true then
        self:addToCmdHist(buf)
    end

    return buf
end

----------------------------------------------------------------
--- restoreTtyState
---
--- Restore the original TTY state
----------------------------------------------------------------
function Prompter:restoreTtyState()
    self:stty(self.initTermState)
end

----------------------------------------------------------------
--- selection
---
--- Routine to prompt the user for a selection. Depending on the
--- types of data  passed in different results occur.
---
--- NOTE: If type(sDefault) == "string" then this DOES add
---       to the command history
---
--- Example, passes a default value and gives a prompt string.
---          This returns a string value.
--- ssid = Menu_Selection( g_tLastNet.ssid, "Enter an SSID:" )
---
--- Example, passes a default entry (string), with a prompt string
---          and a table of valid entries to choose from.  The list
---          will be displayed and the default marked.  The user will
---          select a value from a numeric listing.
---          This returns a string value.
--- key_mgmt = Menu_Selection( g_tLastNet.cli.key_mgmt,
---                            "Select the Key Management:",
---                            {"NONE", "WEP", "WPA-PSK", "WPA2-PSK", "WPA-PSK + WPA2-PSK"} )
---
--- Example, passes a nil default entry, with a prompt string
---          and a table of valid entries to choose from.  The list
---          will be displayed and "NONE" is marked as default.
---          The user will select a value from a numeric listing.
---          This returns a string value.
--- key_mgmt = Menu_Selection( nil,
---                            "Select the Key Management:",
---                            {"NONE", "WEP", "WPA-PSK", "WPA2-PSK", "WPA-PSK + WPA2-PSK"} )
---
--- Example, same as above, except no default is selected.
--- key_mgmt = Menu_Selection( {},
---                            "Select the Key Management:",
---                            {"NONE", "WEP", "WPA-PSK", "WPA2-PSK", "WPA-PSK + WPA2-PSK"} )
---
--- Example, passes a default value (int) and gives a prompt string.
---          This returns a numeric value.
--- id = Menu_Selection( g_tLastNet.id or 0, "Enter an ID:" )
---
--- Example, passes a default value (boolean) and gives a prompt string.
---          This returns a boolean value.
--- bScan = Menu_Selection( true, "Enable Scanning:" )
---
--- Example, passes NO default value, only a prompt string.
---          This returns a string value.
--- sOption = Menu_Selection( nil, "Option to Change:" )
---
--- @tparam   (string|number|boolean|table|nil)
---                                    sDefault ....default value
---                                    if nil:
---                                    tList[1] is the default.
---                                    if table:
---                                    sDefault.default...default value (same
---                                    as not useing a table, if missing
---                                    there is no default.
---
--- @tparam   (string)                 sPrompt  ....user prompt string
--- @tparam   (table)                  tList    ....optional list of possible
---                                                 values to display
--- @tparam   (boolean)                bRetIndexOnly...only relevant when tList
---                                                 isnon-nil.  true returns the
---                                                 item number in the list the
---                                                 user selected--not the string.
---
--- @treturn  (string|number|boolean) Users selection.  They type
---                                   returned is based off of sDefault.
----------------------------------------------------------------
function Prompter:selection( sDefault, sPrompt, tList, bRetIndexOnly )

    local iDefault = 1

    if type(sDefault) == "table" then
        sDefault = sDefault.default
        if sDefault == nil then
            -- NO default
            iDefault = nil
        end
    elseif sDefault ~= nil then
        iDefault = nil
    end

    if type(tList) == "table" then

        if iDefault ~= nil then
            sDefault = tList[iDefault]
        end

        -- Ask until we get something reasonable
        local bGotResult = false
        local sVal       = ""
        while bGotResult == false do
            print(sPrompt)
            for idx,name in pairs(tList) do
                if sDefault == name then
                    iDefault = idx
                    print(idx,name, "(default)")
                else
                    print(idx,name)
                end
            end
            print(string.rep("-",30))
            sVal = self:getLine(true)
            if sVal == nil or sVal == "" or tonumber(sVal) == nil then
                if iDefault then
                    bGotResult = true
                    print(tList[iDefault])
                    sVal = tostring(iDefault)
                end
            else
                bGotResult = true
            end

            if bGotResult then
                break
            end

        end -- loop

        print()
        if bRetIndexOnly == true then
            return tonumber(sVal) -- return the index
        else
            return tList[tonumber(sVal)]
        end
    end

    if sDefault then
        print(string.format("%s (default: %s)",sPrompt, tostring(sDefault)))
    else
        print(sPrompt)
    end
    local sVal = self:getLine(true)
    if (sVal == nil or sVal == "") and sDefault then
        print(sDefault)
        sVal = sDefault
    end

    print()

    if type(sDefault) == "number" then
        if tonumber(sVal) == nil then
            return sDefault
        end
        return tonumber(sVal)
    end

    if type(sDefault) == "boolean" then
        if sVal == "true" then
            return true
        elseif sVal == "false" then
            return false
        end

        return sDefault
    end

    if type(sDefault) == "string" and tonumber(sDefault) == nil and
       sVal ~= "" and tonumber(sVal) == nil then
        self:addToCmdHist(sVal)
    end

    return sVal

end

-----------------------------------------------

-- Uniform method to get the version (useful when compiled)
if not package.loaded[...] and arg[1] == "--version" then
    -- MUST be running as an executable, not a loaded module
    print(string.format("Version: %s\n", VERSION))
    os.exit(1)
end

-----------------------------------------------

return Prompter



