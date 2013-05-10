#!/usr/bin/env lua
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
@file           reg_test.lua
@author         Tysen Moore
@brief          Standalone regression tests for the l2dbus module.
*****************************************************************************
--]]

local helpText = [[

  Standalone regression tests for the l2dbus module.

  Command line arguments:
  -g              -- Continue on failure
  -h              -- This help
  --count [count] -- number of times to repeat tests (default = 1)
  --version       -- Get the application version and exit
  -v              -- Debug verbosity (add more v's fr more verbosity)

]]
------------------------------------------------------------------------------

local cjson     = require "cjson.safe"
local pretty     = require "pl.pretty"

local l2dbus    = require "l2dbus"
local proxy     = require "l2dbus.proxyctrl"


local APP_VER               = "v1.1"


local TEST_ERRORNAME        = "com.l2dbusTester.error.outOfSomething"
local TEST_INTERFACE        = "com.l2dbusTester.ServiceProvider"
local TEST_BUSNAME          = "com.l2dbusTester.service.app"
local TEST_OBJPATH          = "/com/l2dbusTester/service/app"
local TEST_METHOD           = "RequestName"
local TEST_SIGNAL           = "SignalName"
local TEST_SENDER           = "com.l2dbusTester.service.appTx"

local TEST_SERIAL           = 137


local g_exitOnFailure       = true    -- for easier debug set to true
local g_iterations          = 1       -- number of iterations to execute (-i)
local g_curIter             = 0       -- current iteration
local g_verbose             = 0

local g_failed              = 0
local g_passed              = 0
local g_tests               = 0

local g_sectionExecCount    = 0

local g_testName            = ""
local g_sectionName         = ""

-- test execution stack
-- This is a list of objects that allow us to process the complete protocol.
-- e.g.
-- {{ "tx" : "openSession",      payload={} },
--  { "rx" : "openSession_RESP", payload={"sessionId":1}, ... }
local g_testStack           = {}


-- Forward Declaration
local CallMethod, deepcompare, DumpTestStack



----------------------------------------------------------------
--- GetCallerInfo
---
--- Get the caller source and line number
---
--- @tparam   (string)  depth ....if nil, caller or the routine that
---                               called this function
---                               or, use values larger than 3 to
---                               go deeper in the call stack
---
--- @treturn  (string) "Called: filename:lineNum".  For example,
---                    "Called: @/Path/to/File.lua:123"
----------------------------------------------------------------
local function GetCallerInfo( depth )

    if depth == nil then
        depth = 3
    end

    local dbgInfo = debug.getinfo(depth, "Sl")
    local ret     = ""
    if dbgInfo then
        ret = string.format("Called: %s:%d",
                            dbgInfo.source,
                            dbgInfo.currentline)
    end
    return ret

end -- GetCallerInfo

----------------------------------------------------------------
--- PrintIncFailure
---
--- Print a failure and increment the fail count
---
--- @tparam   (string)  fmt  ....format or string to log
--- @tparam   (string)  ...  ....args to string.format (optional)
----------------------------------------------------------------
local function PrintIncFailure( fmt, ... )

    g_failed = g_failed + 1

    print(string.rep("v", 40))

    if g_iterations > 1 then
        print("Iteration:         ", string.format("%d/%d", g_curIter, g_iterations))
    end

    print(string.format(fmt, unpack(arg)))

    print(debug.traceback())

    print(string.rep("^", 40))

    if g_exitOnFailure == true then
        os.exit(1)
    end

end -- PrintIncFailure

----------------------------------------------------------------
--- TestMethod
---
--- Uniform handling for calling some routines
---
--- @tparam   (string)  sTestId      ....
--- @tparam   (string)  failureLogic ...."~=" or "=="
--- @tparam   (string)  failureVal   ....
--- @tparam   (string)  method       ....
--- @tparam   (string)  ...          ....
---
--- @treturn  (boolean) [add here]
----------------------------------------------------------------
local function TestMethod( sTestId, failureLogic, failureVal, method, ... )

    local bRet          = true
    local retVal        = method(unpack(arg))

    g_sectionExecCount  = g_sectionExecCount + 1

    if failureLogic == "~=" and retVal ~= failureVal then
        print()
        print(retVal, "~=", failureVal)
        PrintIncFailure("%s ERROR: %s failed (~=)", g_testName, sTestId)
        bRet = false

    elseif failureLogic == "==" and retVal == failureVal then
        print()
        print(retVal, "==", failureVal)
        PrintIncFailure("%s ERROR: %s failed (==)", g_testName, sTestId)
        bRet = false
    end

    if bRet == true then
        g_passed = g_passed + 1
    end

    return bRet, retVal

end -- TestMethod

----------------------------------------------------------------
--- TestPcallMethod
---
--- Uniform handling for calling some routines that require a pcall
---
--- @tparam   (string)  sTestId      ....
--- @tparam   (boolean) bPcallOK     ....expected pcall OK flag
--- @tparam   (string)  failureLogic ...."~=" or "=="
--- @tparam   (string)  failureVal   ....
--- @tparam   (string)  method       ....
--- @tparam   (string)  ...          ....
---
--- @treturn  (boolean) [add here]
----------------------------------------------------------------
local function TestPcallMethod( sTestId, bPcallOK, failureLogic, failureVal, method, ... )

    local bRet          = true
    local bOK, retVal   = pcall( method, unpack(arg))

    g_sectionExecCount  = g_sectionExecCount + 1

    if bOK ~= bPcallOK then
        PrintIncFailure("%s ERROR: %s failed (bOK:~=)", g_testName, sTestId)
        bRet = false
    else
        if failureLogic == "~=" and retVal ~= failureVal then
            print(retVal, "~=", failureVal)
            PrintIncFailure("%s ERROR: %s failed (~=)", g_testName, sTestId)
            bRet = false

        elseif failureLogic == "==" and retVal == failureVal then
            print(retVal, "==", failureVal)
            PrintIncFailure("%s ERROR: %s failed (==)", g_testName, sTestId)
            bRet = false
        end
    end

    if bRet == true then
        g_passed = g_passed + 1
    end

    return bRet, retVal

end -- TestPcallMethod

----------------------------------------------------------------
--- CallMethod
---
--- [add description here]
---
--- @tparam   (string)  bus           ....For logging, this will show a caller of the
--- @tparam   (string)  callerName    ....request (e.g. "nav", etc)
---                                       unique ID of test (for error reporting)
--- @tparam   (string)  testID        ....name of method to call
--- @tparam   (string)  reqName       ....data to pass to the method
--- @tparam   (string)  reqData       ....nil=expected to pass, else expected to fail
--- @tparam   (string)  expectError   ....true=caller will process the response for
--- @tparam   (string)  bCallerChecks ....success or failure else, we will
---                                       determine failure/pass here
---
--- @treturn  (string) response from call
--- @treturn  (table)  table formatted response from call
--- @treturn  (string) test ID passed in (easier for callers reporting errors)
----------------------------------------------------------------
function CallMethod( bus,
                     callerName,
                     testID,
                     reqName,
                     reqData,
                     expectError,
                     bCallerChecks )

    local bus   = nil
    local tRespData

    g_tests = g_tests + 1

    assert(reqName, "Missing request name")

    if testID == nil then
        testID = string.format("%s.%s(%s(%s))", g_testName, reqName, callerName, reqData)
    end

    -- Add this call to the stack
    local tReqData = cjson.decode( reqData )

    --print("TX:", cjson.encode({tx=reqName, payload = tReqData}))
    table.insert( g_testStack, {tx=reqName, payload = tReqData} )

    local sResp, sErr, tRespData = bus.request(reqName, reqData)

    if sResp == nil then

        -- Add this call to the stack
        --print("ERR:", cjson.encode({rx=string.format("%s_RESP",reqName), payload = sErr}))
        table.insert( g_testStack, {rx=string.format("%s_RESP",reqName), payload = sErr} )

        if bCallerChecks ~= true then
            if expectError then
                g_passed = g_passed + 1
            else
                PrintIncFailure("%s ERROR: Expected to pass but failed (sResp=%s)", tostring(testID), sErr)
            end
        else
            -- Good for finding differences in passed vs total
            --print("Caller Checks1:", testID, reqName, GetCallerInfo())
        end
        sResp = sErr
        -- Let the normal/common processing do its thing

    else
        --print("RX:", cjson.encode({rx=string.format("%s_RESP",reqName), payload = sResp}))
        table.insert( g_testStack, {rx=string.format("%s_RESP",reqName), payload = tRespData} )
    end


    -- check the response
    local tResp, errStr = cjson.decode( sResp )

    if errStr then
        PrintIncFailure("%s ERROR: Bad JSON format on response (%s)", testID, sResp)
        return nil, nil, testID
    end

    if bCallerChecks ~= true then
        if tResp and tResp.errCode and tResp.errName and tResp.errMsg then
            -- ERROR
            if expectError then
                g_passed = g_passed + 1
            else
                DumpTestStack(string.format("Current Test Stack on Error (%s, %s):", g_testName, g_sectionName))
                PrintIncFailure("%s ERROR: Expected to pass but failed (sResp=%s)", tostring(testID), sResp)
                sResp = nil
            end
        else
            -- PASSED
            if expectError then
                DumpTestStack(string.format("Current Test Stack on Error (%s, %s):", g_testName, g_sectionName))
                PrintIncFailure("%s ERROR: Expected failure but passed (sResp=%s)", tostring(testID), sResp)
                sResp = nil
            else
                g_passed = g_passed + 1
            end
        end
    else
        -- Good for finding differences in passed vs total
        --print("Caller Checks2:", dbusMode, testID, reqName, GetCallerInfo())
    end

    return sResp, tResp, testID

end -- CallMethod

----------------------------------------------------------------
--- deepcompare
---
--- From: http://snippets.luacode.org/snippets/Deep_Comparison_of_Two_Values_3
--- with modifications specific to our needs.
---
--- LICENSE: MIT/X11
---
--- @tparam   (string)  t1        ....expected table
--- @tparam   (string)  t2        ....table to compare against
--- @tparam   (string)  ignore_mt ....true, ingnore metatable __eq
---
--- @treturn  (boolean) true=t1 is in t2,
---                     false=t1 not completely in t2
--- @treturn  (string)  error string
----------------------------------------------------------------
function deepcompare(t1,t2,ignore_mt)

    local depth = 0

    local function doCompare(t1,t2,ignore_mt)
        local ty1       = type(t1)
        local ty2       = type(t2)
        local sErr      = nil
        local lineIdx   = 1

        if ty1 ~= ty2 then
            return false, string.format("expected.type(%s) ~= testStack.type(%s)", ty1,ty2), lineIdx
        end
        -- non-table types can be directly compared
        if ty1 ~= 'table' and ty2 ~= 'table' then
            if t1 ~= t2 then
                sErr = string.format("%s ~= %s", tostring(t1),tostring(t2))
            end
            return t1 == t2, sErr, lineIdx
        end
        -- as well as tables which have the metamethod __eq
        if not ignore_mt then
            local mt = getmetatable(t1)
            if mt and mt.__eq then
                if t1 ~= t2 then
                    sErr = string.format("%s ~= %s", tostring(t1),tostring(t2))
                end
                return t1 == t2, sErr, lineIdx
            end
        end

        for k1,v1 in pairs(t1) do
            local v2 = t2[k1]
            local bRet

            if v2 ~= nil then
                bRet, sErr = doCompare(v1,v2,ignore_mt)
            end

            if v2 == nil or bRet == false  then
                if v2 == nil then
                    sErr = string.format("testStack[%s] is missing", k1)
                elseif type(v1) ~= "table" then
                    sErr = string.format("expected[%s](%s) ~= teststack[%s](%s)",
                                        k1,tostring(v1), k1,tostring(v2))
                end
                return false, sErr, lineIdx
            end
            lineIdx = lineIdx + 1
        end
        return true, nil
    end

    local bRet, sErr, iLine = doCompare(t1,t2,ignore_mt, depth)

    if sErr then
        sErr = string.format("%s ==> Line: %d", sErr, iLine)
    end

    return bRet, sErr, iLine

end -- deepcompare

----------------------------------------------------------------
--- deepcopy
---
--- This function recursively copies a table's contents, and
--  ensures that metatables are preserved                                              <br>
---                                                                                    <br>
--- Code taken from:
--- <a href="http://snippets.luacode.org/snippets/Deep_copy_of_a_Lua_Table_2">Here</a> <br>
---                                                                                    <br>
--- LICENSE: MIT/X11                                                                   <br>
---
--- @tparam   (string)  t ....table to copy
---
--- @treturn  (table) copied table
----------------------------------------------------------------
local function deepcopy(t)
    if type(t) ~= 'table' then return t end
    local mt = getmetatable(t)
    local res = {}
    for k,v in pairs(t) do
        if type(v) == 'table' then
            v = deepcopy(v)
        end
        res[k] = v
    end
    setmetatable(res,mt)
    return res
end -- deepcopy

----------------------------------------------------------------
--- DumpTestStack
---
--- Allows us to do special processing to make
-- it compact, yet readable.
---
--- @tparam   (string)  comment     ....optional string to log
--- @tparam   (string)  tOtherStack ....nil, dumps global test stack
---                                     else dumps table passed in.
--- @tparam   (string)  iErrLine    ....line number of the error
--- @tparam   (string)  bSessionObj ....set to false for easier debugging
---
--- @treturn  (string) stack in string format
----------------------------------------------------------------
function DumpTestStack( comment, tOtherStack, iErrLine, bSessionObj )

    local sLine = ""
    local sRet  = ""
    local stack = tOtherStack or g_testStack

    local function add( s, pre )
        print((pre or "")..s)
        sRet = sRet..s.."\n"
    end

    if iErrLine == nil then
        iErrLine = 0
    end

    if g_verbose > 1 then
        -- neaten this later, pretty dump is not compact enough
        add( string.rep("=", 50) )
        if comment then
            add( string.format("--- %s ---", comment) )
        end

        for i, obj in ipairs( stack ) do
            local name = obj.rx or obj.tx

            if i==iErrLine then
                add( string.rep("v", 50) )
            end

            add( string.format("%s=%q, payload=%s",
                                  (obj.rx) and "rx" or "tx",
                                  name,
                                  cjson.encode(obj.payload) or "nil") )

            if i==iErrLine then
                add( string.rep("^", 50) )
            end

        end -- loop
        add( string.rep("=", 50) )
    end

    return sRet

end -- DumpTestStack

----------------------------------------------------------------
--- ParseArgs
---
--- Parse command line arguments
----------------------------------------------------------------
local function ParseArgs()

    ----------------------------------------------------------------
    --- getopt
    ---
    --- Same functionality as "getopt" and "getopt_long"
    --- in BSD/Unix.                                                       <br>
    ---                                                                    <br>
    --- "getopt() translated from the BSD
    --- getopt(); compatible with the default
    --- Unix getopt()"                                                     <br>
    ---                                                                    <br>
    --- getopt_long() is meant to match the BSD
    --- implementation.                                                    <br>
    ---                                                                    <br>
    ---************************************************                    <br>
    --- IMPORTANT: This call will remove entries
    ---            from the args list passed in.
    ---            If this is a problem make a
    ---            deepcopy() of the args before
    ---            calling this routine.                                   <br>
    ---************************************************                    <br>
    ---                                                                    <br>
    --- Code taken from:                                                   <br>
    --- <a href="https://github.com/attractivechaos/klib/blob/master/lua/klib.lua">Here</a>   <br>
    ---                                                                    <br>
    --- Then added:                                                        <br>
    --- + long argument parsing on our own                                 <br>
    --- + pass back unknown arguments to continue the parsing process      <br>
    ---   or allow you to terminate parsing--more flexible.
    --- + Added debug parameter for better debugging                       <br>
    ---                                                                    <br>
    --- LICENSE: MIT                                                       <br>
    ---
    --- @tparam   (string)  args     ....argument table to parse (args are removed!)
    --- @tparam   (string)  ostr     ....string of options
    --- @tparam   (table)   long_opt ....optional table of long options
    ---                                  (see example above)
    --- @tparam   (boolean) bDebug   ....true enables debug logging, else no logging
    ---
    --- @treturn  (string) option name, nil will terminate the loop. if "?"
    ---                    then the option is unknown and "options value" is the
    ---                    unknown option.
    --- @treturn  (string) option value
    ----------------------------------------------------------------
    local function getopt(args, ostr, long_opt, bDebug)
        local arg, place = nil, 0;
        local iDebug     = (bDebug == true) and 1 or 0
        return function ()
            if place == 0 then -- update scanning pointer
                place = 1
                if #args == 0 or args[1]:sub(1, 1) ~= '-' then
                    place = 0;

                    if #args == 0 then
                        --print("getopt: nil (shortopt): end of args")
                        return nil
                    else
                        local badArg = table.remove(args, 1)
                        print(string.format("getopt: ? (shortopt): %s", badArg))
                        return "?", badArg
                    end
                end
                if #args[1] >= 2 then
                    place = place + 1
                    if args[1]:sub(2, 2) == '-' then -- found "--"

                        -- Handle the long option

                        -- This will remove "--"
                        local longArg = table.remove(args, 1):sub(3);
                        local retOpt  = nil
                        local optVal  = nil                                      --> lcov: ref-1

                        if long_opt then
                            for i, longObj in ipairs(long_opt) do
                                if longArg == longObj[1] then

                                    if longObj[3] then
                                        retOpt = longObj[3] -- use this value
                                    else
                                        retOpt = longObj[1] -- keep original name
                                    end

                                    if longObj[2] == true then
                                        -- requires another parameter
                                        optVal = table.remove(args, 1)
                                    end
                                    break;
                                end
                            end
                        end

                        place = 0
                        if retOpt == nil then
                            print(string.format("getopt: ? (longopt): %s", longArg))
                            return "?", longArg
                        else
                            print(string.format("getopt: retOpt/optVal: %s/%s", retOpt, optVal))
                            return retOpt, optVal
                        end

                    end
                end
            end
            local optopt = args[1]:sub(place, place);
            place = place + 1;
            local oli = ostr:find(optopt);
            if optopt == ':' or oli == nil then -- unknown option
                if optopt == '-' then
                    print("getopt: nil (unknown option)")
                    return nil
                end
                if place > #args[1] then
                    table.remove(args, 1);
                    place = 0;
                end
                print(string.format("getopt: ? (unknown option): %s", optopt))
                return '?', optopt;
            end
            oli = oli + 1;
            if ostr:sub(oli, oli) ~= ':' then -- do not need argument
                arg = nil;
                if place > #args[1] then
                    table.remove(args, 1);
                    place = 0;
                end
            else -- need an argument
                if place <= #args[1] then  -- no white space
                    arg = args[1]:sub(place);
                else
                    table.remove(args, 1);
                    if #args == 0 then -- an option requiring argument is the last one
                        place = 0;
                        if ostr:sub(1, 1) == ':' then
                            print("getopt: :")
                            return ':'
                        end
                        print("getopt: ? (needs arg)")
                        return '?';
                    else arg = args[1] end
                end
                table.remove(args, 1);
                place = 0;
            end
            print(string.format("getopt: optopt/arg: %s/%s", optopt, arg))
            return optopt, arg;
        end

    end -- getopt ------------------------------------------------


    local longOpt = {--name         hasArg   short/retOpt
                     {"count",      true,    nil },
                     {"version",    false,   nil } }
    local lArgs   = deepcopy(arg)

    for opt, optval in getopt(lArgs, 'ghsv', longOpt) do

        if opt == "g" then
            g_exitOnFailure = false

        elseif opt == "h" then
            print()
            print(arg[0], APP_VER)
            print(helpText)
            os.exit(1)

        elseif opt == "count" then
            g_iterations = tonumber(optval)
            if g_iterations == nil then
                g_iterations = 1
            end

        elseif opt == "v" then
            g_verbose = g_verbose + 1

        elseif opt == "version" then
            print(string.format("Version: %s\n", APP_VER))
            os.exit(1)
        end

    end -- arg loop

    if g_verbose > 0 then
        print("Verbose:         ", g_verbose)
        print("Exit on Failure: ", tostring(g_exitOnFailure))
        print("Iterations:      ", g_iterations)
    end

end -- ParseArgs

----------------------------------------------------------------
--- PrintResults
---
--- Print result stats
---
--- @tparam   (string)  bClear ....true=clear stats
----------------------------------------------------------------
local function PrintResults( bClear )

    print("\n**********************************************")
    print("Total Tests:", g_tests)
    print("Passed:     ", g_passed)
    print("Failed:     ", g_failed)


    if bClear == true then
        g_passed = 0
        g_failed = 0
        g_tests  = 0
    end

end -- PrintResults

----------------------------------------------------------------
--- PrintStackError
---
--- Define only what you want to compare
---
--- @tparam   (string)  comment        ....
--- @tparam   (string)  tExpectedStack ....
--- @tparam   (string)  iErrLine       ....line of the error
--- @tparam   (string)  bSessionObj    ....set to false for easier debugging
---
--- @treturn  (boolean) false == error, true == success
----------------------------------------------------------------
local function PrintStackError( comment, tExpectedStack, iErrLine, bSessionObj )

    local sLine = ""
    local sLog  = ""

    local function add( s, pre )
        print((pre or "")..s)
        sLog  = sLog..s.."\n"
    end

    g_failed = g_failed + 1

    add( string.rep("|", 50), "\n" )
    add( string.rep("v", 50))

    add( string.format("ERROR:         %s", comment) )

    add( string.rep("-", 50) )

    add( string.format("ERROR STACK: %s", GetCallerInfo()) )
    add( string.format("ERROR STACK: %s", GetCallerInfo(4)) )

    sLine = DumpTestStack("Expected a stack of this",  tExpectedStack, iErrLine, bSessionObj)
    sLog  = sLog..sLine.."\n"

    add( string.rep("|", 50) )

    sLine = DumpTestStack("Stack after test was this", nil,            iErrLine, bSessionObj)
    sLog  = sLog..sLine.."\n"

    add( string.rep("^", 50) )
    add( string.rep("|", 50) )

    if g_exitOnFailure == true then
        os.exit(1)
    end

end -- PrintStackError

----------------------------------------------------------------
--- Test_l2dbus_ConnAPI
---
--- Tests simple argument passing to the connection API/interfaces
----------------------------------------------------------------
local function Test_l2dbus_ConnAPI()

    local bOK       = true
    local bSuccess  = true
    local conn
    local disp      = l2dbus.Dispatcher.new()
    local errStr    = ""

    -- Section init
    g_testName          = "Test_ConnAPI"
    g_sectionExecCount  = 0

    print(g_testName.." tests start")

    -- open
    ----------------------------------------------
    -- Bad args...
    local testArgs = { { 1, -1, "BAD", {"val",1}, true, "nil", deepcopy }, -- expects: userdata
                       { 1, -1, {"val",1}, true, "nil", deepcopy },        -- expects: string
                       { 1, -1, "BAD", {"val",1}, "nil", deepcopy },       -- expects: bool
                       { 1, -1, "BAD", {"val",1}, "nil", deepcopy } }      -- expects: bool
    for argX=1,4 do

        for idx, val in pairs(testArgs[argX]) do
            if val == "nil" then
                val = nil
            end

            -- MUST pcall, otherwise it just errors out
            if argX == 1 then
                bOK, conn = pcall( l2dbus.Connection.open, val,  "someAddress")
            elseif argX == 2 then
                bOK, conn = pcall( l2dbus.Connection.open, disp, val)
            elseif argX == 3 then
                bOK, conn = pcall( l2dbus.Connection.open, disp, l2dbus.Dbus.BUS_SYSTEM, val)
            elseif argX == 4 then
                bOK, conn = pcall( l2dbus.Connection.open, disp, l2dbus.Dbus.BUS_SYSTEM, false, val)
            end

            g_sectionExecCount = g_sectionExecCount + 1
            if bOK == true then
                PrintIncFailure("Test_ConnAPI ERROR: open.1.arg%d.%d failed", argX,idx)
            else
                g_passed = g_passed + 1
            end

        end -- bad arg loop

    end -- argX loop

    -- openStandard
    ----------------------------------------------
    -- Bad args...
    local testArgs = { { 1, -1, "BAD", {"val",1}, true, "nil", deepcopy }, -- expects: userdata
                       { -1, "BAD", {"val",1}, true, "nil", deepcopy },    -- expects: number
                       { 1, -1, "BAD", {"val",1}, "nil", deepcopy },       -- expects: bool
                       { 1, -1, "BAD", {"val",1}, "nil", deepcopy } }      -- expects: bool
    for argX=1,4 do

        for idx, val in pairs(testArgs[argX]) do
            if val == "nil" then
                val = nil
            end

            -- MUST pcall, otherwise it just errors out
            if argX == 1 then
                bOK, conn = pcall( l2dbus.Connection.openStandard, val,  l2dbus.Dbus.BUS_SYSTEM)
            elseif argX == 2 then
                bOK, conn = pcall( l2dbus.Connection.openStandard, disp, val)
            elseif argX == 3 then
                bOK, conn = pcall( l2dbus.Connection.openStandard, disp, l2dbus.Dbus.BUS_SYSTEM, val)
            elseif argX == 4 then
                bOK, conn = pcall( l2dbus.Connection.openStandard, disp, l2dbus.Dbus.BUS_SYSTEM, false, val)
            end

            g_sectionExecCount = g_sectionExecCount + 1
            if bOK == true then
                PrintIncFailure("Test_ConnAPI ERROR: openStandard.1.arg%d.%d failed", argX,idx)
            else
                g_passed = g_passed + 1
            end

        end -- bad arg loop

    end -- argX loop

    -- Good call
    bSuccess, conn = TestMethod( "isConnected.1(openStandard)",
                                 "==", nil,         -- fail when?
                                 l2dbus.Connection.openStandard,
                                 disp, l2dbus.Dbus.BUS_SYSTEM )
    if bSuccess == false then
        print("ERROR: Exiting Early")
        return g_sectionExecCount
    else

        -- NEEDS WORK......
--      -- connection object calls, non-conn self test
--      local connMethods = { "isConnected",       "isAuthenticated",       "isAnonymous",
--                            "getServerId",       "getBusId",              "getDescriptor",
--                            "canSendType",       "getDescriptor",         "send",
--                            "sendWithReply",     "sendWithReplyAndBlock", "flush",
--                            "hasMessagesToSend", "registerMatch",         "unregisterMatch",
--                            "registerServiceObject", "unregisterServiceObject"}
--      local testArg     = { -1, "BAD", {"val",1}, true, "nil" }
--      for _, val in pairs(testArg) do
--          if val == "nil" then
--              val = nil
--          end
--
--          for _, method in pairs(connMethods) do
--              g_sectionExecCount = g_sectionExecCount + 1
--              --bOK, errStr = pcall( l2dbus.Connection[method], val)
--              bOK, errStr = pcall( l2dbus.Connection.isConnected, conn)
--              if bOK == true then
--                  PrintIncFailure("Test_ConnAPI ERROR: connTest - %s(%s) failed", method, val)
--              else
--                  print("ERR", errStr, conn:isConnected())
--                  g_passed = g_passed + 1
--              end
--          end
--      end -- bad conn testing


        -- isConnected
        g_sectionExecCount  = g_sectionExecCount + 1
        if conn:isConnected() == false then
            PrintIncFailure("%s ERROR: isConnected.1(isConnected) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        -- isAuthenticated
        g_sectionExecCount  = g_sectionExecCount + 1
        if conn:isAuthenticated() == false then
            PrintIncFailure("%s ERROR: isConnected.1(isConnected) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        -- isAnonymous
        g_sectionExecCount  = g_sectionExecCount + 1
        if conn:isAnonymous() == false then
            PrintIncFailure("%s ERROR: isConnected.1(isAnonymous) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        -- getServerId
        g_sectionExecCount  = g_sectionExecCount + 1
        if conn:getServerId() == nil then
            PrintIncFailure("%s ERROR: isConnected.1(getServerId) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        -- getBusId
        g_sectionExecCount  = g_sectionExecCount + 1
        if conn:getBusId() == nil then
            PrintIncFailure("%s ERROR: isConnected.1(getBusId) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        -- getDescriptor
        g_sectionExecCount  = g_sectionExecCount + 1
        if conn:getDescriptor() == nil then
            PrintIncFailure("%s ERROR: isConnected.1(getDescriptor) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        -- canSendType
        -- expected to fail, thus the pcall
        g_sectionExecCount  = g_sectionExecCount + 1
        if pcall(conn.canSendType) == true then
            PrintIncFailure("%s ERROR: isConnected.1(canSendType.0) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        -- canSendType
        g_sectionExecCount  = g_sectionExecCount + 1
        local fd = conn:getDescriptor()
        if fd == nil or type(fd) ~= "number" then
            PrintIncFailure("%s ERROR: isConnected.1(canSendType.0) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        local tTypes = {}
        for idx,sType in pairs(tTypes) do
            g_sectionExecCount  = g_sectionExecCount + 1
            if conn:canSendType(sType) ~= true then
                PrintIncFailure("%s ERROR: isConnected.1(canSendType.%d) failed", g_testName, idx)
            else
                g_passed = g_passed + 1
            end
        end -- tType loop

        -- hasMessagesToSend
        g_sectionExecCount  = g_sectionExecCount + 1
        if conn:hasMessagesToSend() ~= false then
            PrintIncFailure("%s ERROR: isConnected.1(hasMessages) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        -- flush
        g_sectionExecCount  = g_sectionExecCount + 1
        if pcall(conn.flush, conn) == false then
            PrintIncFailure("%s ERROR: isConnected.1(flush) failed", g_testName)
        else
            g_passed = g_passed + 1
        end

        -- send
        local testArg = { -1, "BAD", {"val",1}, true, "nil" }
        for _, val in pairs(testArg) do
            if val == "nil" then
                val = nil
            end

            g_sectionExecCount = g_sectionExecCount + 1
            bOK, errStr = pcall( conn.send, conn, val)
            if bOK ~= false then
                PrintIncFailure("Test_ConnAPI ERROR: isConnected.1(send(%s)) failed", val)
            else
                g_passed = g_passed + 1
            end
        end -- bad arg testing

        -- send, sendWithReply, sendWithReplyAndBlock
        -- tests passing a bad first argument (msg).
        local testArg = { -1, "BAD", {"val",1}, true, "nil", deepcopy  }
        for _, val in pairs(testArg) do
            if val == "nil" then
                val = nil
            end

            -- Array of the l2dbus.Connection.xxx does not work, so plan B
            for idx=1,6 do
                local method = ""
                g_sectionExecCount = g_sectionExecCount + 1
                if idx == 1 then
                    bOK, errStr = pcall( conn.send, conn, val)
                    method = "send"
                elseif idx == 2 then
                    bOK, errStr = pcall( conn.sendWithReply, conn, val)
                    method = "sendWithReply"
                elseif idx == 3 then
                    bOK, errStr = pcall( conn.sendWithReplyAndBlock, conn, val)
                    method = "sendWithReplyAndBlock"
                elseif idx == 4 then
                    bOK, errStr = pcall( conn.unregisterMatch, conn, val)
                    method = "unregisterMatch"
                elseif idx == 5 then
                    bOK, errStr = pcall( conn.registerServiceObject, conn, val)
                    method = "registerServiceObject"
                elseif idx == 6 then
                    bOK, errStr = pcall( conn.unregisterServiceObject, conn, val)
                    method = "unregisterServiceObject"
                end
                if bOK ~= false then
                    PrintIncFailure("Test_ConnAPI ERROR: isConnected.1(%s(%s)) failed", method, val)
                else
                    g_passed = g_passed + 1
                end
            end
        end -- bad arg testing

        --**************************************************
        --**************************************************
        -- Not covered here:
        -- open(no success test)
        -- send(partly), flush,
        -- sendWithReply(partly), sendWithReplyAndBlock(partly),
        -- registerMatch, unregisterMatch,
        -- registerServiceObject, unregisterServiceObject
        --**************************************************
        --**************************************************

    end -- if good connection testing

    print(string.format("%s tests done (exec=%d)", g_testName, g_sectionExecCount))

    return g_sectionExecCount

end -- Test_l2dbus_ConnAPI

----------------------------------------------------------------
--- Test_MessageAPI
---
--- Tests simple argument passing to the message API/interfaces
----------------------------------------------------------------
local function Test_MessageAPI( )

    local bSuccess  = true
    local msg
    local bOK

    local tIfaceErrors   = { -1, "BAD", {"val",1}, true, "nil", deepcopy,
                             "..bad.format", "oneElement",
                             ".bad.format", "com.$23.invalid",
                             "com."..string.rep("a", 255)   }
    local tObjPathErrors = { 1, -1, "BAD", {"val",1}, "nil", deepcopy,
                             TEST_BUSNAME, "com/bad/format", "/com//wrong",
                             "/com/wrong/", "/com/wrong/$23" }

    local tBusNameErrors = { 1, -1, "BAD", {"val",1}, "nil", deepcopy,
                             TEST_OBJPATH, "..bad.format", "oneElement",
                             ".bad.format", "com.$23.invalid",
                             "com."..string.rep("a", 255) }


    -- Section init
    g_testName          = "Test_MessageAPI"
    g_sectionExecCount  = 0

    -- - - - - - - - - - - - - - - - - - - - - - - - - -
    -- Generic means to test complete API any message
    function testMsg( testName, msg, iMsgType )

        -- msg:getType
        -- - - - - - - - - - - -
        g_sectionExecCount  = g_sectionExecCount + 1
        if msg:getType() ~= iMsgType then
            PrintIncFailure("%s ERROR: msg:getType() failed", testName)
        else
            g_passed = g_passed + 1
        end

        -- msg:setNoReply/getNoReply
        -- - - - - - - - - - - - - - -
        g_sectionExecCount  = g_sectionExecCount + 2
        msg:setNoReply(false)
        if msg:getNoReply() ~= false then
            PrintIncFailure("%s ERROR: msg:setType(false) failed", testName)
        else
            g_passed = g_passed + 2
        end

        g_sectionExecCount  = g_sectionExecCount + 2
        msg:setNoReply(true)
        if msg:getNoReply() ~= true then
            PrintIncFailure("%s ERROR: msg:setType(true) failed", testName)
        else
            g_passed = g_passed + 2
        end

        -- Already set to true, should not change
        -- Test BAD values...
        for idx, val in pairs( { -1, "BAD", {"val",1}, "nil", deepcopy} ) do
            if val == "nil" then
                val = nil
            end
            g_sectionExecCount  = g_sectionExecCount + 2
            if pcall( msg.setNoReply, msg, val) == true then
                PrintIncFailure("%s ERROR: msg:setType.bad.%d(%s) passed, but should have failed", testName, idx, tostring(val))
            else
                if msg:getNoReply() ~= true then
                    PrintIncFailure("%s ERROR: msg:setType.bad.%d(%s) failed", testName, idx, tostring(val))
                else
                    g_passed = g_passed + 2
                end
            end
        end -- loop

        -- msg:setAutoStart/getAutoStart
        -- - - - - - - - - - - - - - - - -
        g_sectionExecCount  = g_sectionExecCount + 2
        msg:setAutoStart(false)
        if msg:getAutoStart() ~= false then
            PrintIncFailure("%s ERROR: msg:setAutoStart(false) failed", testName)
        else
            g_passed = g_passed + 2
        end

        g_sectionExecCount  = g_sectionExecCount + 2
        msg:setAutoStart(true)
        if msg:getAutoStart() ~= true then
            PrintIncFailure("%s ERROR: msg:setAutoStart(true) failed", testName)
        else
            g_passed = g_passed + 2
        end

        -- Already set to true, should not change
        -- Test BAD values...
        for idx, val in pairs( { -1, "BAD", {"val",1}, "nil", deepcopy} ) do
            if val == "nil" then
                val = nil
            end
            g_sectionExecCount  = g_sectionExecCount + 1
            if pcall( msg.setAutoStart, msg, val) == true then
                PrintIncFailure("%s ERROR: msg:setAutoStart.bad.%d(%s) passed, but should have failed", testName, idx, tostring(val))
            else
                g_passed            = g_passed + 1
                g_sectionExecCount  = g_sectionExecCount + 1
                if msg:getAutoStart() ~= true then
                    PrintIncFailure("%s ERROR: msg:setAutoStart.bad.%d(%s) failed", testName, idx, tostring(val))
                else
                    g_passed = g_passed + 1
                end
            end
        end -- loop

        -- msg:getObjectPath/hasObjectPath
        -- - - - - - - - - - - - - - - - -
        if iMsgType == l2dbus.Message.METHOD_CALL or
           iMsgType == l2dbus.Message.METHOD_CALL then

            g_sectionExecCount = g_sectionExecCount + 1
            if msg:hasObjectPath( l2dbus.Dbus.PATH_DBUS ) ~= true then
                PrintIncFailure("%s ERROR: msg:hasObjectPath() failed", testName)
            else
                g_passed           = g_passed + 1 -- from hasObjectPath()
                g_sectionExecCount = g_sectionExecCount + 1
                if msg:getObjectPath() ~= l2dbus.Dbus.PATH_DBUS then
                    PrintIncFailure("%s ERROR: msg:getObjectPath(false) failed", testName)
                else
                    g_passed = g_passed + 1
                end
            end

            -- msg:setObjectPath (BAD values)
            -- - - - - - - - - - - - - - - - -
            for idx, val in pairs( tObjPathErrors ) do
                -- Skip nil since allowed by  this interface
                if val ~= "nil" then
                    g_sectionExecCount  = g_sectionExecCount + 1
                    if pcall( msg.setObjectPath, msg, val) == true then
                        PrintIncFailure("%s ERROR: msg:setObjectPath.bad.%d(%s) passed, but should have failed", testName, idx, tostring(val))
                    else
                        g_sectionExecCount  = g_sectionExecCount + 1
                        if msg:getObjectPath() ~= l2dbus.Dbus.PATH_DBUS then
                            PrintIncFailure("%s ERROR: msg:setObjectPath.bad.%d(%s) failed", testName, idx, tostring(val))
                        else
                            g_passed = g_passed + 1
                        end
                    end
                end
            end -- loop

            -- msg:setObjectPath (GOOD)
            -- - - - - - - - - - - - - - - - -
            g_sectionExecCount  = g_sectionExecCount + 1
            msg:setObjectPath( TEST_OBJPATH )
            g_sectionExecCount  = g_sectionExecCount + 1
            if msg:getObjectPath() ~= TEST_OBJPATH then
                PrintIncFailure("%s ERROR: msg:setObjectPath.good(%s) failed, got(%s)", testName, TEST_OBJPATH, msg:getObjectPath() or "nil")
            else
                g_passed = g_passed + 1
            end

            -- msg:getDecomposedObjectPath
            -- - - - - - - - - - - - - - - - -
            g_sectionExecCount  = g_sectionExecCount + 1
            local tPath = msg:getDecomposedObjectPath()
            if tPath == nil or "/"..table.concat(tPath, "/") ~= TEST_OBJPATH then
                PrintIncFailure("%s ERROR: msg:getDecomposedObjectPath(%s) failed, got(%s)", testName, TEST_OBJPATH, tPath and "/"..table.concat(tPath, "/") or "nil")
            else
                g_passed = g_passed + 1
            end

        end -- MethodCall or Signal

        if iMsgType == l2dbus.Message.METHOD_CALL or
           iMsgType == l2dbus.Message.SIGNAL then

            -- msg:getInterface/hasInterface
            -- - - - - - - - - - - - - - - - -
            g_sectionExecCount = g_sectionExecCount + 1
            if msg:hasInterface( l2dbus.Dbus.INTERFACE_DBUS ) ~= true then
                PrintIncFailure("%s ERROR: msg:hasInterface() failed", testName)
            else
                g_passed           = g_passed + 1 -- from hasInterface()
                g_sectionExecCount = g_sectionExecCount + 1
                if msg:getInterface() ~= l2dbus.Dbus.INTERFACE_DBUS then
                    PrintIncFailure("%s ERROR: msg:getInterface() failed, got(%s)", testName, msg:getInterface() or "nil")
                else
                    g_passed = g_passed + 1
                end
            end

            -- msg:setInterface (BAD values)
            -- - - - - - - - - - - - - - - - -
            for idx, val in pairs( tIfaceErrors ) do
                -- Skip nil since allowed by  this interface
                if val ~= "nil" then
                    g_sectionExecCount  = g_sectionExecCount + 1
                    if pcall( msg.setInterface, msg, val) == true then
                        PrintIncFailure("%s ERROR: msg:setInterface.bad.%d(%s) passed, but should have failed", testName, idx, tostring(val))
                    else
                        g_sectionExecCount  = g_sectionExecCount + 1
                        if msg:getInterface() ~= l2dbus.Dbus.INTERFACE_DBUS then
                            PrintIncFailure("%s ERROR: msg:setInterface.bad.%d(%s) failed", testName, idx, tostring(val))
                        else
                            g_passed = g_passed + 1
                        end
                    end
                end
            end -- loop

            -- msg:setInterface (GOOD)
            -- - - - - - - - - - - - - - - - -
            g_sectionExecCount  = g_sectionExecCount + 1
            msg:setInterface( TEST_INTERFACE )
            g_sectionExecCount  = g_sectionExecCount + 1
            if msg:getInterface() ~= TEST_INTERFACE then
                PrintIncFailure("%s ERROR: msg:setInterface.good(%s) failed got(%s)", testName, TEST_INTERFACE, msg:getInterface() or "nil")
            else
                g_passed = g_passed + 1
            end

        end -- MethodCall or Signal

        local sExpectedDest = nil
        if iMsgType == l2dbus.Message.METHOD_RETURN or
           iMsgType == l2dbus.Message.ERROR then
            sExpectedDest = TEST_SENDER
        elseif iMsgType == l2dbus.Message.METHOD_CALL then
            sExpectedDest = l2dbus.Dbus.SERVICE_DBUS
        end

        if iMsgType == l2dbus.Dbus.MESSAGE_TYPE_SIGNAL then

            -- Check for failed attempts

            g_sectionExecCount = g_sectionExecCount + 1
            if msg:hasDestination( l2dbus.Dbus.SERVICE_DBUS ) ~= false then
                PrintIncFailure("%s ERROR: msg:hasDestination() failed", testName)
            else
                g_passed           = g_passed + 1 -- from hasDestination()
                g_sectionExecCount = g_sectionExecCount + 1
                if msg:getDestination() ~= nil then
                    PrintIncFailure("%s ERROR: msg:getDestination() failed, got(%s)", testName, msg:getDestination() or "nil")
                else
                    g_passed = g_passed + 1
                end
            end

        else -- MethodCall, MethodReturn, Error

            -- msg:getDestination/hasDestination
            -- - - - - - - - - - - - - - - - - - -
            g_sectionExecCount = g_sectionExecCount + 1
            if msg:hasDestination( sExpectedDest ) ~= true then
                PrintIncFailure("%s ERROR: msg:hasDestination() failed, got(%s)", testName, msg:getDestination() or "nil")
            else
                g_passed           = g_passed + 1 -- from hasDestination()
                g_sectionExecCount = g_sectionExecCount + 1
                if msg:getDestination() ~= sExpectedDest then
                    PrintIncFailure("%s ERROR: msg:getDestination() failed, got(%s)", testName, msg:getDestination() or "nil")
                else
                    g_passed = g_passed + 1
                end
            end

        end

        -- msg:setDestination (BAD values)
        -- - - - - - - - - - - - - - - - - - -
        for idx, val in pairs( tBusNameErrors ) do
            -- Skip nil since allowed by  this interface
            if val ~= "nil" then
                g_sectionExecCount  = g_sectionExecCount + 1
                if pcall( msg.setDestination, msg, val) == true then
                    PrintIncFailure("%s ERROR: msg:setDestination.bad[%d](%s) passed, but should have failed", testName, idx, tostring(val))
                else
                    g_sectionExecCount  = g_sectionExecCount + 1
                    if msg:getDestination() ~= sExpectedDest then
                        PrintIncFailure("%s ERROR: msg:setDestination.bad[%d](%s) failed, got(%s)", testName, idx, tostring(val), msg:getDestination() or "nil")
                    else
                        g_passed = g_passed + 1
                    end
                end
            end
        end -- loop

        -- msg:setDestination (GOOD)
        -- - - - - - - - - - - - - - - - -
        g_sectionExecCount  = g_sectionExecCount + 1
        msg:setDestination(TEST_BUSNAME)
        g_sectionExecCount  = g_sectionExecCount + 1
        if msg:getDestination() ~= TEST_BUSNAME then
            PrintIncFailure("%s ERROR: msg:setDestination.good(%s) failed got(%s)", testName, TEST_BUSNAME, msg:getDestination() or "nil")
        else
            g_passed = g_passed + 1
        end

        local sMember = TEST_METHOD
        if iMsgType == l2dbus.Dbus.MESSAGE_TYPE_SIGNAL then
            sMember = TEST_SIGNAL
        end

        if iMsgType == l2dbus.Message.METHOD_RETURN or
           iMsgType == l2dbus.Message.ERROR then

            -- Simple test for failure
            g_sectionExecCount  = g_sectionExecCount + 1
            if pcall( msg.setMember, msg, TEST_METHOD) == true then
                PrintIncFailure("%s ERROR: msg:setMember(%s) passed, but should have failed", testName, TEST_METHOD)
            else
                g_passed = g_passed + 1
            end

            g_sectionExecCount  = g_sectionExecCount + 1
            if pcall( msg.getMember, msg) == true then
                PrintIncFailure("%s ERROR: msg:getMember(%s) passed, but should have failed", testName, TEST_METHOD)
            else
                g_passed = g_passed + 1
            end

            g_sectionExecCount  = g_sectionExecCount + 1
            if pcall( msg.hasMember, msg, TEST_METHOD) == true then
                PrintIncFailure("%s ERROR: msg:hasMember(%s) passed, but should have failed", testName, TEST_METHOD)
            else
                g_passed = g_passed + 1
            end

        else -- MethodCall, Signal

            -- msg:getMember/hasMember
            -- - - - - - - - - - - - - - - - - - -
            g_sectionExecCount = g_sectionExecCount + 1
            if msg:hasMember( sMember ) ~= true then
                PrintIncFailure("%s ERROR: msg:hasMember() failed", testName)
            else
                g_passed           = g_passed + 1 -- from hasInterface()
                g_sectionExecCount = g_sectionExecCount + 1
                if msg:getMember() ~= sMember then
                    PrintIncFailure("%s ERROR: msg:getMember() failed, got(%s)", testName, msg:getMember() or "nil")
                else
                    g_passed = g_passed + 1
                end
            end

            -- msg:setMember (BAD values)
            -- - - - - - - - - - - - - - - - - - -
            for idx, val in pairs( { -1, {"val",1}, true, deepcopy } ) do
                g_sectionExecCount  = g_sectionExecCount + 1
                if pcall( msg.setMember, msg, val) == true then
                    PrintIncFailure("%s ERROR: msg:setMember.bad.%d(%s) passed, but should have failed", testName, idx, tostring(val))
                else
                    g_sectionExecCount  = g_sectionExecCount + 1
                    if msg:getMember() == val then
                        PrintIncFailure("%s ERROR: msg:setMember.bad.%d(%s) failed, got(%s)", testName, idx, tostring(val), msg:getMember() or "nil")
                    else
                        g_passed = g_passed + 1
                    end
                end
            end -- loop

            -- msg:setMember (GOOD)
            -- - - - - - - - - - - - - - - - -
            g_sectionExecCount  = g_sectionExecCount + 1
            msg:setMember(sMember.."2")
            g_sectionExecCount  = g_sectionExecCount + 1
            if msg:getMember() ~= sMember.."2" then
                PrintIncFailure("%s ERROR: msg:setMember.good(%s) failed got(%s)", testName, sMember.."2", msg:getMember() or "nil")
            else
                g_passed = g_passed + 1
            end
        end

        -- msg:setErrorName
        -- - - - - - - - - - - - - - - - -
        if iMsgType == l2dbus.Dbus.MESSAGE_TYPE_ERROR then

            -- msg:setErrorName (BAD)
            -- - - - - - - - - - - - - - - - -
            -- (same restrictions as interface name)
            for idx, val in pairs( tIfaceErrors ) do
                -- Skip nil since allowed by  this interface
                if val ~= "nil" then
                    g_sectionExecCount  = g_sectionExecCount + 1
                    if pcall( msg.setErrorName, msg, val) == true then
                        PrintIncFailure("%s ERROR: msg:setErrorName.bad.%d(%s) passed, but should have failed", testName, idx, tostring(val))
                    else
                        g_sectionExecCount  = g_sectionExecCount + 1
                        if msg:getErrorName() ~= TEST_ERRORNAME then
                            PrintIncFailure("%s ERROR: msg:setErrorName.bad.%d(%s) failed, got(%s)", testName, idx, tostring(val), msg:getErrorName() or "nil")
                        else
                            g_passed = g_passed + 1
                        end
                    end
                end
            end -- loop

            -- msg:setErrorName (GOOD)
            -- - - - - - - - - - - - - - - - -
            g_sectionExecCount  = g_sectionExecCount + 1
            msg:setErrorName(TEST_BUSNAME)
            g_sectionExecCount  = g_sectionExecCount + 1
            if msg:getErrorName() ~= TEST_BUSNAME then
                PrintIncFailure("%s ERROR: msg:setDestination.good(%s) failed got(%s)", testName, TEST_BUSNAME, msg:getErrorName() or "nil")
            else
                g_passed = g_passed + 1
            end

        else -- MethodCall, MethodReturn, Signal

            -- Simple test for failure
            g_sectionExecCount  = g_sectionExecCount + 1
            if pcall( msg.setErrorName, msg, TEST_ERRORNAME) == true then
                PrintIncFailure("%s ERROR: msg:setErrorName(%s) passed, but should have failed", testName, TEST_ERRORNAME)
            else
                g_passed = g_passed + 1
            end

            g_sectionExecCount  = g_sectionExecCount + 1
            if pcall( msg.getErrorName, msg) == true then
                PrintIncFailure("%s ERROR: msg:getErrorName(%s) passed, but should have failed", testName, TEST_ERRORNAME)
            else
                g_passed = g_passed + 1
            end
        end

        -- msg:setSender (GOOD)
        -- - - - - - - - - - - - - - - - -
        g_sectionExecCount  = g_sectionExecCount + 1
        msg:setSender(TEST_SENDER)
        g_sectionExecCount  = g_sectionExecCount + 1
        if msg:getSender() ~= TEST_SENDER then
            PrintIncFailure("%s ERROR: msg:setSender.good(%s) failed got(%s)", testName, TEST_SENDER, msg:getSender() or "nil")
        else
            g_passed = g_passed + 1
        end

        -- msg:hasSender (GOOD)
        -- - - - - - - - - - - - - - - - -
        g_sectionExecCount  = g_sectionExecCount + 1
        if msg:hasSender(TEST_SENDER) ~= true then
            PrintIncFailure("%s ERROR: msg:hasSender.good(%s) failed got(%s)", testName, TEST_SENDER, msg:getSender() or "nil")
        else
            g_passed = g_passed + 1
        end

        -- msg:setSender (BAD values)
        -- - - - - - - - - - - - - - - - - - -
        for idx, val in pairs( tBusNameErrors ) do
            -- Skip nil since allowed by  this interface
            if val ~= "nil" then
                g_sectionExecCount  = g_sectionExecCount + 1
                if pcall( msg.setSender, msg, val) == true then
                    PrintIncFailure("%s ERROR: msg:setSender.bad.%d(%s) passed, but should have failed", testName, idx, tostring(val))
                else
                    g_sectionExecCount  = g_sectionExecCount + 1
                    if msg:getSender() ~= TEST_SENDER then
                        PrintIncFailure("%s ERROR: msg:setSender.bad.%d(%s) failed, got(%s)", testName, idx, tostring(val), msg:getSender() or "nil")
                    else
                        g_passed = g_passed + 1
                    end
                end
            end
        end -- loop

        -- msg:setSerial (GOOD)
        -- - - - - - - - - - - - - - - - -
        g_sectionExecCount  = g_sectionExecCount + 1
        msg:setSerial(TEST_SERIAL)
        g_sectionExecCount  = g_sectionExecCount + 1
        if msg:getSerial() ~= TEST_SERIAL then
            PrintIncFailure("%s ERROR: msg:setSerial.good(%d) failed got(%d)", testName, TEST_SERIAL, msg:getSerial() or "nil")
        else
            g_passed = g_passed + 1
        end

        -- msg:setSerial (BAD values)
        -- - - - - - - - - - - - - - - - - - -
        for idx, val in pairs( { "string", {"val",1}, true, "nil", deepcopy } ) do
            if val == "nil" then
                val = nil
            end

            g_sectionExecCount  = g_sectionExecCount + 1
            if pcall( msg.setSerial, msg, val) == true then
                PrintIncFailure("%s ERROR: msg:setSerial.bad.%d(%s) passed, but should have failed", testName, idx, tostring(val))
            else
                g_sectionExecCount  = g_sectionExecCount + 1
                if msg:getSerial() ~= TEST_SERIAL then
                    PrintIncFailure("%s ERROR: msg:setSerial.bad.%d(%s) failed, got(%s)", testName, idx, tostring(val), msg:getSerial() or "nil")
                else
                    g_passed = g_passed + 1
                end
            end
        end -- loop

    end -- testMsg
    -- - - - - - - - - - - - - - - - - - - - - - - - - -

    print(g_testName.." tests start")

    -- Equivalant constants
    -----------------------------
    if deepcompare(
        { l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL,
          l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN,
          l2dbus.Dbus.MESSAGE_TYPE_ERROR,
          l2dbus.Dbus.MESSAGE_TYPE_SIGNAL },
        { l2dbus.Message.METHOD_CALL,
          l2dbus.Message.METHOD_RETURN,
          l2dbus.Message.ERROR,
          l2dbus.Message.SIGNAL } ) == false then
        PrintIncFailure("%s ERROR: constants differ", g_testName)
    else
        g_passed = g_passed + 1
    end

    -- new (Bad Args)
    -----------------------------
    for idx=-1,10 do
        if idx ~= l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL and
           idx ~= l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN and
           idx ~= l2dbus.Dbus.MESSAGE_TYPE_ERROR and
           idx ~= l2dbus.Dbus.MESSAGE_TYPE_SIGNAL then
            bSuccess, msg = TestPcallMethod( g_testName.."(new.bad)."..tostring(idx),
                                             false,
                                             "==", nil,         -- fail when?
                                             l2dbus.Message.new,
                                             idx )
            if bSuccess == false then
                print("ERROR: Exiting Early")
                return g_sectionExecCount
            end
        end
    end -- for loop

    -- new (Good Args)
    -----------------------------
    local tTypes = { l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL,
                     l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN,
                     l2dbus.Dbus.MESSAGE_TYPE_ERROR,
                     l2dbus.Dbus.MESSAGE_TYPE_SIGNAL }
    for idx, mtype in pairs(tTypes) do
        bSuccess, msg = TestPcallMethod( g_testName.."(new.ok)."..tostring(idx),
                                         true,
                                         "==", nil,         -- fail when?
                                         l2dbus.Message.new,
                                         mtype )
        msg = nil  -- cleanup
        if bSuccess == false then
            print("ERROR: Exiting Early")
            return g_sectionExecCount
        end
    end -- for loop


    -- newMethodCall(arg list) (Bad Args)
    -----------------------------------------
    print(string.rep("v",60))
    print("NOTE: Expected error output below...")
    local testArgs = { { 1, -1, true, "nil", deepcopy }, -- expects: string or table
                       tIfaceErrors,
                       tObjPathErrors,
                       tBusNameErrors }
    for argX=1,4 do

        for idx, val in pairs(testArgs[argX]) do
            if val == "nil" then
                val = nil
            end

            -- MUST pcall, otherwise it just errors out
            if argX == 1 then
                bOK = pcall( l2dbus.Message.newMethodCall, val, TEST_INTERFACE, TEST_OBJPATH, TEST_BUSNAME)
            elseif argX == 2 then
                bOK = pcall( l2dbus.Message.newMethodCall, "Method", val, TEST_OBJPATH, TEST_BUSNAME)
            elseif argX == 3 then
                bOK = pcall( l2dbus.Message.newMethodCall, "Method", TEST_INTERFACE, val, TEST_BUSNAME)
            elseif argX == 4 then
                bOK = pcall( l2dbus.Message.newMethodCall, "Method", TEST_INTERFACE, TEST_OBJPATH, val)
            end

            g_sectionExecCount = g_sectionExecCount + 1
            if bOK == true then
                PrintIncFailure(g_testName.." ERROR: newMethodCall(argList).arg%d.%d failed", argX,idx)
            else
                g_passed = g_passed + 1
            end
        end

    end -- argX loop

    -- newMethodCall(table) (Bad Args)
    -----------------------------------------
    -- testing only bad values
    for argX=1,4 do

            for idx, val in pairs(testArgs[argX]) do
            local bSkipIter = false

            if val == "nil" then
                val = nil
            end

            local tArg = { }
            local sArg = ""
            if argX == 1 then
                tArg.destination = TEST_BUSNAME
                tArg.path        = TEST_OBJPATH
                tArg.interface   = TEST_INTERFACE
                tArg.method      = val
                sArg             = "method"

                -- Skip valid tests, only test errors
                if type(val) == "table" then
                    bSkipIter = true
                end

            elseif argX == 2 then
                tArg.destination = TEST_BUSNAME
                tArg.path        = TEST_OBJPATH
                tArg.interface   = val
                tArg.method      = "Method"
                sArg             = "interface"

                -- Skip valid tests, only test errors
                if val == nil then
                    bSkipIter = true
                end

                --  TODO:   Remove these when the code is fixed.
                --  ERRORS: According to the spec these should fail, skipped here to continue testing
                if type(val) == "table" or
                   type(val) == "boolean" or
                   type(val) == "function" or
                   type(val) == "number" then
                    bSkipIter = true
                end

            elseif argX == 3 then
                tArg.destination = TEST_BUSNAME
                tArg.path        = val
                tArg.interface   = TEST_INTERFACE
                tArg.method      = "Method"
                sArg             = "path"

            elseif argX == 4 then
                tArg.destination = val
                tArg.path        = TEST_OBJPATH
                tArg.interface   = TEST_INTERFACE
                tArg.method      = "Method"
                sArg             = "destination"

                -- Skip valid tests, only test errors
                if val == nil then
                    bSkipIter = true
                end

                --  TODO:   Remove these when the code is fixed.
                --  ERRORS: According to the spec these should fail, skipped here to continue testing
                if type(val) == "table" or
                   type(val) == "function" or
                   type(val) == "number" then
                    bSkipIter = true
                end

            end

            if bSkipIter == false then
                -- MUST pcall, otherwise it just errors out
                bOK = pcall( l2dbus.Message.newMethodCall, tArg)

                g_sectionExecCount = g_sectionExecCount + 1
                if bOK == true then
                    PrintIncFailure(g_testName.." ERROR: newMethodCall(table).%s[%d] passed, but should have failed (arg: %s)", sArg,idx, tostring(val))
                else
                    g_passed = g_passed + 1
                end
            end

        end -- val loop

    end -- argX loop

    -- Signal Methods
    ----------------------------
    -- Arg List format
    bSuccess, sig = TestMethod( g_testName.."(Signal--argList)",
                                "==", nil,         -- fail when?
                                l2dbus.Message.newSignal,
                                l2dbus.Dbus.PATH_DBUS,
                                l2dbus.Dbus.INTERFACE_DBUS,
                                TEST_SIGNAL )
    if bSuccess == false or sig == nil then
        print("ERROR: Exiting Early")
        return g_sectionExecCount
    end
    sig = nil -- cleanup

    -- Table format
    bSuccess, sig = TestMethod( g_testName.."(Signal--table)",
                                "==", nil,         -- fail when?
                                l2dbus.Message.newSignal,
                                {  path        = l2dbus.Dbus.PATH_DBUS,
                                   interface   = l2dbus.Dbus.INTERFACE_DBUS,
                                   name        = TEST_SIGNAL } )

    if bSuccess == false or sig == nil then
        print("ERROR: Exiting Early")
        return g_sectionExecCount
    end
    testMsg( g_testName.."(Signal)", sig, l2dbus.Dbus.MESSAGE_TYPE_SIGNAL )

    -- Method Call Methods
    ----------------------------
    -- Arg List format
    bSuccess, msg = TestMethod( g_testName.."(MethodCall--argList)",
                                "==", nil,         -- fail when?
                                l2dbus.Message.newMethodCall,
                                l2dbus.Dbus.SERVICE_DBUS,
                                l2dbus.Dbus.PATH_DBUS,
                                l2dbus.Dbus.INTERFACE_DBUS,
                                TEST_METHOD )
    if bSuccess == false or msg == nil then
        print("ERROR: Exiting Early")
        return g_sectionExecCount
    end
    msg = nil -- cleanup

    -- Table format
    bSuccess, msg = TestMethod( g_testName.."(MethodCall--table)",
                                "==", nil,         -- fail when?
                                l2dbus.Message.newMethodCall,
                                {  destination = l2dbus.Dbus.SERVICE_DBUS,
                                   path        = l2dbus.Dbus.PATH_DBUS,
                                   interface   = l2dbus.Dbus.INTERFACE_DBUS,
                                   method      = TEST_METHOD} )

    if bSuccess == false or msg == nil then
        print("ERROR: Exiting Early")
        return g_sectionExecCount
    end
    testMsg( g_testName.."(MethodCall)", msg, l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL )

    -- Method Return Methods
    ----------------------------
    bSuccess, reply = TestMethod( g_testName.."(MethodReturn)",
                                "==", nil,         -- fail when?
                                l2dbus.Message.newMethodReturn,
                                msg )
    if bSuccess == false or reply == nil then
        print("ERROR: Exiting Early")
        return g_sectionExecCount
    end
    testMsg( g_testName.."(MethodReturn)", reply, l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN )

    -- Method Error Methods (BAD)
    ----------------------------
    local testArgs = { { 1, -1, true, "BAD", {"val",1}, "nil", deepcopy }, -- expects: userdata
                       tIfaceErrors,                                       -- expects: string
                       { 1, -1, true, {"val",1}, deepcopy } }              -- expects: string or nil
    for argX=1,3 do

        for idx, val in pairs(testArgs[argX]) do
            if val == "nil" then
                val = nil
            end

            -- TODO: Remove when fixed
            if argX == 3 and (idx == 1 or idx == 2) then
                val = deepcopy
            end

            -- MUST pcall, otherwise it just errors out
            if argX == 1 then
                bOK = pcall( l2dbus.Message.newError, val, TEST_ERRORNAME)
            elseif argX == 2 then
                bOK = pcall( l2dbus.Message.newError, msg, val)
            elseif argX == 3 then
                bOK = pcall( l2dbus.Message.newError, msg, TEST_ERRORNAME, val)
            end

            g_sectionExecCount = g_sectionExecCount + 1
            if bOK == true then
                PrintIncFailure(g_testName.." ERROR: newError(bad).arg%d.%d failed (arg: %s)", argX,idx, tostring(val))
            else
                g_passed = g_passed + 1
            end
        end

    end -- argX loop

    -- Method Error Methods (GOOD)
    -------------------------------
    bSuccess, errReply = TestMethod( g_testName.."(MethodError)",
                                     "==", nil,         -- fail when?
                                     l2dbus.Message.newError,
                                     msg, TEST_ERRORNAME, "errMsg" )
    if bSuccess == false or errReply == nil then
        print("ERROR: Exiting Early")
        return g_sectionExecCount
    end
    testMsg( g_testName.."(MethodError)", errReply, l2dbus.Dbus.MESSAGE_TYPE_ERROR )


    --[[   TODO: Test these:
    getSignature (msg)
    hasSignature (msg, signature)
    containsUnixFds (msg)
    addArgs (msg, ...)
    addArgsBySignature (msg, signature, ...)
    getArgs (msg)
    getArgsAsArray (msg)
    marshallToArray (msg)
    ]]


    --  msg:addArgsBySignature( "su",
    --                          "org.example.service.AudioCapture",
    --                          l2dbus.Dbus.NAME_FLAG_DO_NOT_QUEUE )


    print(string.format("%s tests done (exec=%d)", g_testName, g_sectionExecCount))

    -- Some cleanup
    msg         = nil
    errReply    = nil

    return g_sectionExecCount

end -- Test_MessageAPI

----------------------------------------------------------------
--- Test_MessageArgsAPI
---
--- Tests simple argument passing to the message API/interfaces
----------------------------------------------------------------
local function Test_MessageArgsAPI( )

    local bSuccess  = true
    local msg
    local bOK

    -- Section init
    g_testName          = "Test_MessageAPI"
    g_sectionExecCount  = 0







    print(string.format("%s tests done (exec=%d)", g_testName, g_sectionExecCount))

    -- Some cleanup
    msg         = nil
    errReply    = nil

    return g_sectionExecCount

end -- Test_MessageArgsAPI




----------------------------------------------------------------
--- main
---
--- Start testing
----------------------------------------------------------------
local function main( )

    while g_curIter ~= g_iterations do
        g_curIter = g_curIter + 1

        print("**********************************************")
        print(string.format("Starting tests %s...", APP_VER))
        if g_iterations > 1 then
            print("Iteration:", g_curIter)
        end
        print("**********************************************")

        -- Iface argument handling tests
        local sOut     = ""
        local apiTests = { { Test_l2dbus_ConnAPI, "l2dbus Conn Tests:          %d\n" },
                           { Test_MessageAPI,     "Message Tests:              %d\n" },
                           { Test_MessageArgsAPI, "Message Marshalling Tests:  %d\n"} }
        for _,testObj in pairs( apiTests ) do

            g_sectionExecCount = 0
            testObj[1]()

            g_tests = g_sectionExecCount + g_tests

            sOut = sOut..string.format(testObj[2], g_sectionExecCount)

        end -- API test loop

        PrintResults()
        print("----------------------------------------------")
        print(sOut)
        if g_iterations > 1 then
            print("Iteration:         ", string.format("%d/%d", g_curIter, g_iterations))
        end
        print("**********************************************")
    end -- iteration loop

    print()

    -- fall back into the audio mgr with open connections
    -- to test the RemoveSessions.

end -- main

ParseArgs()
main()
