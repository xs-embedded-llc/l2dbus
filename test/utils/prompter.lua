local ev = require("ev")

local prompterInst = nil
local Prompter = {}
Prompter.__index = Prompter

function Prompter:new(loop)
    if prompterInst == nil then
        local f = {initTermState = "",
                    pending = nil,
                    loop = loop,
                    stdIn = io.stdin,
                    stdOut = io.stdout,
                    stdInIo = nil}
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
        prompterInst.stdInIo = ev.IO.new(stdInHandler, 0, ev.READ)
    end

    return prompterInst
end


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
    self.stdInIo:start(self.loop)
    local value = coroutine.yield()
    self:stty(oldState)
    return value
end

function Prompter:getLine()
    local oldState = self:stty(self.initTermState)
    self.stdInIo:stop(self.loop)
    self.pending = nil
    local line = self.stdIn:read("*l")
    self:stty(oldState)
    return line
end

function Prompter:restoreTtyState()
    self:stty(self.initTermState)
end

----------------------------------------------------------------
--- selection
---
--- Routine to prompt the user for a selection. Depending on the
--- types of data  passed in different results occur.
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
            sVal = self:getLine()
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
    local sVal = self:getLine()
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

    return sVal

end

return Prompter