#!/usr/bin/env lua
------------------------------------------------------------------------------
-- l2dbus Stress Tester
--
------------------------------------------------------------------------------

local helpText = [[

  This tool is used to stress the l2dbus interface using the shim API.  This
  tool implements a simple ping protocol which is configurable for various
  testing scenarios.

  Usage:
  Run one side as the service side using "-S", then run one or more clients
  without the "-S" option.

  Command line arguments:

  Common Args:
  --bus [busName]       -- Bus name
  -g                    -- force garbage collection call after each reply (rx/tx)
                           May impact overall performance.  Default is off.
  -v                    -- Verbosity 
                           v = level 1, Basic Count 
                               Shows the packet count only
                           vv = level 2, Basic I/O 
                               client: 't' == tx (request)
                                       'r' == rx on reply interface (reply)
                                       'e' == rx on error interface (error)
                                       '.' == wait
                               server: '.' == rx/tx on echo interface
                                       'e' == rx/tx on echo as error interface
                                       'r'/'t' == rx/tx on echo async interface
                                       'R'/'T' == rx/tx on echo async as error interface
                           vvv = level 3, verbose output
                               Displays the complete rx/tx payloads

  Client Args:
  -a                    -- Test with an async reply from server. 
                           Default is sync.
  -c [count]            -- Stop after sending [count] packets. 
                           Default is infinite.
  -e                    -- Server will reply on the error interface.
                           Default use normal reply interface.
  -f                    -- Flood  ping.  This will set -i to 0 and enable -r.
  -i [interval]         -- Wait  interval  seconds  between  sending  each 
                           packet.  The default is to wait for one second
                           between each packet normally, or not to wait in 
                           flood mode. Default is 1 second.
  -k                    -- Kill the client after a single reply.  This will
                           result in a new client for each request.  This makes
                           it easier to exercise the client creation code.
                           Default is to use the same client for the whole test.
  -r                    -- This will ignore the reply from the server before 
                           going to the next request/wait/etc.  
                           Default is to wait.
  -s [packetSize]       -- Specifies the number of data bytes to be sent.
                           Default is 64. Minumum is 8 bytes.
  -t                    -- Timestamp and track round trip times. Default is off.
  -w                    -- Wait on Exit. This is useful when using "-c" and
                           then client goes to exit.  This will prevent the 
                           process from cleaning up to allow a chance to look
                           for potential leaks, etc. Default is no wait.

  Server Args:
  -S                    -- Run as the server side. Default is client.

  Example:
  (NOTE: Adjust the verbosity to meet your needs)
  (NOTE: You can run multiple clients for a single server for more stress testing)

  # Stress/stability test of dbus.  
  # (Add "-s" for large packets sizes)
  lua ping_stress.lua -f -v

  # Look for memory leaks.  
  # (Add -a and/or -e for other code paths in l2dbus/cdbus/etc)
  lua ping_stress.lua -c 50 -s 512000 -i 0 -w -g -v

  # Performance testing.  
  # (Add "-s" for other packets sizes, and adjust "-c" longer timing)  
  # NOTE: no verbosity so only calculate the timing and display the stats when done.
  # NOTE: Make sure the service instance has little or no verbosity.
  lua ping_stress.lua -c 50000 -t -i 0

  # Performance testing, this will have the OS do the timing instead of us.
  # (Add "-s" for other packets sizes, and adjust "-c" longer timing)  
  # NOTE: Make sure the service instance has little or no verbosity.
  time lua ping_stress.lua -c 50000 -i 0

]]
------------------------------------------------------------------------------

---------------
-- Requires
---------------
local cjson     = require "cjson.safe"
local pretty    = require "pl.pretty"
local posix     = require "posix"
local ev        = require "ev"
local dbusUtils = require "xse.dbusUtils"
local utils     = require "xse.commonUtils"  -- args parsing

--- If the socket module is not loaded the os.time() will be
--- used which gives a one second granularity instead of milliseconds.
local bOk, socket = pcall( require, "socket")
local g_timeUnits = ""
if bOk == false then
    socket      = nil
    g_timeUnits = "sec"
else
    g_timeUnits = "msec"
end

---------------
-- Const
---------------

local APP_VER               = "1.0.0"

-- Service API
local STRESS_BUSNAME        = "com.l2dbus.service.ping_stress"
local STRESS_OBJPATH        = "/com/l2dbus/service/ping_stress"
local STRESS_INTERFACE      = "com.l2dbus.ping"

local ASYNC_TIMEOUT         = 250 -- msec

---------------
-- Globals
---------------

local g_dbusBusName         = dbusUtils.SESSION_BUS -- --bus option, DBus APP Bus Name
local g_asyncMode           = false             -- -a option
local g_iterCount           = 0                 -- -c option (0 is continuous)
local g_errorIface          = false             -- -e option
local g_floodMode           = false             -- -f option
local g_forceGC             = false             -- -g option
local g_waitInterval        = 1                 -- -i option
local g_killClients         = false             -- -k option
local g_ignoreReply         = false             -- -r option
local g_payloadSize         = 64                -- -s option
local g_serverMode          = false             -- -S option
local g_timestamp           = false             -- -t option
local g_verbose             = 0                 -- -v option
local g_waitOnExit          = false             -- -w option


local g_dbusBus             = nil   -- dbus object

local g_dbMethods           = {}    -- External DBus COMMON interface

local g_testStartTime       = 0
local g_stats               = { iPkt = 0,
                                tx   = 0, txErr = 0, txAsync = 0, txAsyncErr = 0,
                                rx   = 0, rxErr = 0, rxAsync = 0, rxAsyncErr = 0,
                              }

-----------------
-- Function Declr
------------------
local InitDbus, ShutdownDbus, Snapshot


----------------------------------------------------------------
--- log
---
--- Generic/Central logging for most use cases
---
--- @tparam   (string)  sCh     ....minimal (-vv) character(s) to log
--- @tparam   (string)  sLog    ....header to log for -vvv
--- @tparam   (string)  payload ....payload for -vvv
---
--- @treturn  (boolean) [add here]
----------------------------------------------------------------
local function log( sCh, sLog, payload )

    if g_verbose == 1 then
        local sPkt = tostring(g_stats.iPkt-1)
        io.stdout:write(string.rep(string.char(8), #sPkt ))
        io.stdout:write(tostring(g_stats.iPkt))
        io.stdout:flush()

    elseif g_verbose == 2 then
        io.stdout:write(sCh)
        io.stdout:flush()

    elseif g_verbose >= 3 then

        if g_timestamp == true then
            local time = 0
            if socket then
                -- millisecond granularity
                time = socket.gettime() --(socket.gettime()*1000)
            else
                -- second granularity
                time =  os.time()
            end
            print(time .. " - " .. sLog, payload)
        else
            print(sLog, payload)
        end
               
    end

end -- log

----------------------------------------------------------------
-- External Server Interfaces
----------------------------------------------------------------

----------------------------------------------------------------
--- echo
---
--- Responds with the original payload
----------------------------------------------------------------
function g_dbMethods.echo( payload )

    g_stats.iPkt = g_stats.iPkt + 1
    g_stats.rx   = g_stats.rx   + 1
    if g_verbose > 0 then
        log( ".", g_stats.rx .. " - Rx/Tx:echo     ", payload )
    end
    g_stats.tx   = g_stats.tx + 1

    if g_forceGC == true then
        collectgarbage()
    end

    return payload

end -- echo

----------------------------------------------------------------
--- echo_asError
---
--- Responds with the original payload using the error interface
----------------------------------------------------------------
function g_dbMethods.echo_asError( payload )

    g_stats.iPkt  = g_stats.iPkt  + 1
    g_stats.rxErr = g_stats.rxErr + 1
    if g_verbose > 0 then
        log( "e", g_stats.rxErr .. " - Rx/Tx:echoAsErr", payload )
    end
    g_stats.txErr = g_stats.txErr + 1

    if g_forceGC == true then
        collectgarbage()
    end

    return nil, payload

end -- echo_asError

----------------------------------------------------------------
--- echo_async
---
--- Responds with the original payload asynchronously after 
--- a short delay.
----------------------------------------------------------------
function g_dbMethods.echo_async( payload, ctxtContainer  )

    g_stats.iPkt    = g_stats.iPkt    + 1
    g_stats.rxAsync = g_stats.rxAsync + 1
    if g_verbose > 0 then
        log( "r", g_stats.rxAsync .. " - Rx:echoAsync   ", payload )
    end
    
    ev.Timer.new( function() 
                       g_stats.txAsync = g_stats.txAsync + 1
                       if g_verbose > 0 then
                           log( "t", g_stats.txAsync .. " - Tx:echoAsync   ", payload )
                       end
                       local co = coroutine.create( function()
                                ctxtContainer.reply( payload )
                                if g_forceGC == true then
                                    collectgarbage()
                                end
                            end )
                       coroutine.resume(co)
                   end,
                   ASYNC_TIMEOUT/1000, 0 ):start( dbusUtils.getLoop() )

    return nil,nil

end -- echo_async

----------------------------------------------------------------
--- echo_asyncAsError
---
--- Responds with the original payload asynchronously after 
--- a short delay using the error Interface.
----------------------------------------------------------------
function g_dbMethods.echo_asyncAsError( payload, ctxtContainer  )

    g_stats.iPkt       = g_stats.iPkt       + 1
    g_stats.rxAsyncErr = g_stats.rxAsyncErr + 1
    if g_verbose > 0 then
        log( "R", g_stats.rxAsyncErr .. " - Rx:echoAsyncErr", payload )
    end
    
    ev.Timer.new( function() 
                       g_stats.txAsyncErr = g_stats.txAsyncErr + 1
                       if g_verbose > 0 then
                           log( "T", g_stats.txAsyncErr .. " - Tx:echoAsyncErr", payload )
                       end
                       local co = coroutine.create( function()
                                ctxtContainer.replyError( payload )
                                if g_forceGC == true then
                                    collectgarbage()
                                end
                            end )
                       coroutine.resume(co)
                   end,
                   ASYNC_TIMEOUT/1000, 0 ):start( dbusUtils.getLoop() )

    return nil,nil

end -- echo_asyncAsError




----------------------------------------------------------------
-- Other Helper Code
----------------------------------------------------------------


----------------------------------------------------------------
--- ClientRequests
---
--- Starts and executes the requests (pings)
----------------------------------------------------------------
local function ClientRequests()

    -- Optimization when running with "-f"
    local log       = function (sCh, sLog, payload) log(sCh, sLog, payload) end

    local startTime = 0
    local endtime   = 0
    local execTime  = 0

    local sResp     = ""
    local sErr      = ""
    local sCh       = ""
    local bInfinite = (g_iterCount == 0)
    local data      = ""
    local reqName   = "echo"

    if g_asyncMode == true and g_errorIface == true then
        reqName = "echo_asyncAsError"
        data = string.format('{"v":"%s"}', string.rep("A", g_payloadSize-8))
    elseif g_asyncMode == true then
        reqName = "echo_async"
        data = string.format('{"v":"%s"}', string.rep("a", g_payloadSize-8))
    elseif g_errorIface == true then
        reqName = "echo_asError"
        data = string.format('{"v":"%s"}', string.rep("E", g_payloadSize-8))
    else
        data = string.format('{"v":"%s"}', string.rep(".", g_payloadSize-8))
    end

    if g_verbose == 1 then
        io.stdout:write("Pkt: 0")
    end
    
    if g_waitOnExit == true then
        print(string.rep("-", 80))
        print("PRESS <ENTER> TO START...")
        print(string.rep("-", 80))
        io.stdin:read("*line")
    end

    if g_timestamp == true then
        g_stats.time           = {} 
        g_stats.time.avgRTT    = 0
        g_stats.time.maxRTT    = 0
        g_stats.time.minRTT    = 4294967295 -- 0xFFFFFFFF
        g_stats.time.timeUnits = "Seconds"

        if socket then
            -- millisecond granularity
            g_testStartTime = socket.gettime() --(socket.gettime()*1000)
        else
            -- second granularity
            g_testStartTime = os.time()
        end
    end
    
    while bInfinite == true or g_iterCount > 0 do

        g_stats.iPkt = g_stats.iPkt + 1

        if g_killClients == true and g_stats.iPkt ~= 1 then
            g_dbusBus.close()
            InitDbus()
        end
        
        ----------------------------
        -- Pre Logging
        ----------------------------
        log( "t", g_stats.iPkt .. " - Tx:  ", data )

        ----------------------------
        -- Pre Stats
        ----------------------------
        if g_asyncMode == true and g_errorIface == true then
            g_stats.txAsyncErr = g_stats.txAsyncErr + 1
        elseif g_asyncMode == true then
            g_stats.txAsync = g_stats.txAsync + 1
        elseif g_errorIface == true then
            g_stats.txErr = g_stats.txErr + 1
        else
            g_stats.tx = g_stats.tx + 1
        end
        
        ----------------------------
        -- Timing
        ----------------------------
        if g_timestamp == true then
            if socket then
                -- millisecond granularity
                startTime = socket.gettime() --(socket.gettime()*1000)
            else
                -- second granularity
                startTime = os.time()
            end
        end

        ----------------------------------------------------
        -- Do request
        sResp, sErr = g_dbusBus.request(reqName, data, nil, g_ignoreReply)
        ----------------------------------------------------

        ----------------------------
        -- Timing
        ----------------------------
        if g_timestamp == true then
            if socket then
                -- millisecond granularity
                execTime = socket.gettime() - startTime
            else
                -- second granularity
                execTime = os.time() - startTime
            end

            -- Max, Min, Avg
            if execTime > g_stats.time.maxRTT then
                g_stats.time.maxRTT = execTime
            end
            if execTime < g_stats.time.minRTT then
                g_stats.time.minRTT = execTime
            end
            if g_stats.time.avgRTT == 0 then
                g_stats.time.avgRTT = execTime
            end
            g_stats.time.avgRTT = (((g_stats.iPkt-1) * g_stats.time.avgRTT) + execTime)/g_stats.iPkt;
        end

        ----------------------------
        -- Post Stats
        ----------------------------
        if sErr then
            g_stats.rxErr = g_stats.rxErr + 1
        else
            g_stats.rx = g_stats.rx + 1
        end
        
        ----------------------------
        -- Post Logging
        ----------------------------
        if g_verbose > 0 then
            if g_verbose == 1 then
                -- NOP

            elseif g_verbose == 2 then
                if sErr then
                    if g_waitInterval > 0 then
                        io.stdout:write("e.")
                    else
                        io.stdout:write("e")
                    end
                else
                    if g_waitInterval > 0 then
                        io.stdout:write("r.")
                    else
                        io.stdout:write("r")
                    end
                end
                io.stdout:flush()

            elseif g_verbose >= 3 then

                if g_timestamp == true then
                    local time = 0
                    if socket then
                        -- millisecond granularity
                        time = socket.gettime()
                    else
                        -- second granularity
                        time =  os.time()
                    end

                    print(time .. " - " .. g_stats.iPkt .. " - sResp", sResp)
                    print(time .. " - " .. g_stats.iPkt .. " - sErr ", sErr)

                else
                    print(g_stats.iPkt .. " - sResp", sResp)
                    print(g_stats.iPkt .. " - sErr ",  sErr)
                end
                print()
            end
        end -- verbosity
        
        ----------------------------
        -- Post Features
        ----------------------------
        if g_forceGC == true then
            collectgarbage()
        end

        if g_waitInterval > 0 then
            posix.sleep( g_waitInterval )
        end

        if g_iterCount > 0 then
            g_iterCount = g_iterCount - 1
        end        

    end -- loop -------------------------------------

    if socket then
        -- millisecond granularity
        g_stats.time.totalTime = socket.gettime() - g_testStartTime
    else
        -- second granularity
        g_stats.time.totalTime = os.time() - g_testStartTime
    end

    print("\n")
    Snapshot()  -- display stats
    print("\n")

    if g_waitOnExit == true then
        print(string.rep("-", 80))
        print("PRESS <ENTER> TO EXIT...")
        print(string.rep("-", 80))
        io.stdin:read("*line")
    end
    
    ShutdownDbus()

end -- ClientRequests

-----------------------------------------------
-- InitDbus
--
-- Initialize the dbus service and setup the
-- default "request" handler.
--
-- NOTE: Exits on errors
-----------------------------------------------
function InitDbus()

    local errStr = nil
    local result = nil

    g_dbusBus, errStr = dbusUtils.newBus( g_dbusBusName, nil, g_verbose )
    if g_dbusBus == nil then
        print("ERROR: DBus Session Bus not available: ", errStr)
        os.exit(2)
    end

    if g_serverMode == true then
        -- SERVER MODE

        local dbAgent, errStr = g_dbusBus.newAdaptor( STRESS_OBJPATH )
        if errStr then
            print("ERROR: failed to create DBus agent: ", errStr)
            os.exit(3)
        end

        -- Only set the default in case the control logic opens a
        -- new adaptor then this will no longer be the default.
        -- This ensures this will be default when not specified.
        g_dbusBus.setDefaultAdaptor( dbAgent )

        -- Register (generic) method handler
        g_dbusBus.registerGenReqHdlr( g_dbMethods )

        -- Set the connection name
        local bRet, errStr = g_dbusBus.requestName( STRESS_BUSNAME )
        if bRet == false then
            print(string.format("ERROR: Couldn't get the bus name %s => %s", STRESS_BUSNAME, errStr))
            ShutdownDbus()
            os.exit(1)
        end

    else -- CLIENT MODE

        local bRet, errStr = g_dbusBus.requestName( STRESS_BUSNAME.."Client" )
        if bRet == false then
            print(string.format("ERROR: Couldn't get the bus name %s Client => %s", STRESS_BUSNAME, errStr))
            ShutdownDbus()
            os.exit(1)
        end

        local dbProxy, errStr = g_dbusBus.newProxy( STRESS_BUSNAME, STRESS_OBJPATH )
        if dbProxy == nil then
            print("ERROR: failed to create proxy => ", errStr)
            ShutdownDbus()
            os.exit(1)
        end

        -- This ensures this is the default since we will open more latrer
        -- NOTE: This does not mean the deafult is ever used, this is more
        --       for the benifit of testing/plugins.
        g_dbusBus.setDefaultProxy( dbProxy )

    end

end -- InitDbus

----------------------------------------------------------------
--- ParseArgs
---
--- Parse command line arguments
----------------------------------------------------------------
local function ParseArgs()

    local bHelp   = false
    local lArgs   = utils.deepcopy(arg)
    local longOpt = {--name           hasArg   short/retOpt
                     {"bus",          true,    nil },
          }

    for opt, optval in utils.getopt(lArgs, 'ac:efghi:krs:Stvw', longOpt) do

        if opt == "a" then

            g_asyncMode = true

        elseif opt == "bus" then

            g_dbusBusName = optval

        elseif opt == "c" then

            g_iterCount = tonumber(optval)
            if g_iterCount == nil or g_iterCount < 1 then
                print("ERROR: Invalid iteration count (option -c): ", g_iterCount)
                os.exit(1)
            end

        elseif opt == "e" then

            g_errorIface = true

        elseif opt == "f" then

            g_waitInterval = 0
            g_ignoreReply  = true

        elseif opt == "g" then

            g_forceGC = true

        elseif opt == "h" then

            bHelp = true

        elseif opt == "i" then

            g_waitInterval = tonumber(optval)
            if g_waitInterval == nil or g_waitInterval < 0 then
                print("ERROR: Invalid wait interval (option -i): ", g_waitInterval)
                os.exit(1)
            end

        elseif opt == "k" then

            g_killClients = true

        elseif opt == "r" then

            g_ignoreReply = true

        elseif opt == "s" then

            g_payloadSize = tonumber(optval)
            if g_payloadSize == nil or g_payloadSize < 8 then
                print("ERROR: Invalid payload size (option -s): ", g_payloadSize)
                os.exit(1)
            end
            
        elseif opt == "S" then

            g_serverMode = true

        elseif opt == "t" then

            g_timestamp = true

        elseif opt == "v" then

            g_verbose = g_verbose + 1

        elseif opt == "w" then

            g_waitOnExit = true

        end

    end -- arg loop

    -- prevent specifying the same bus name on cmd line and
    -- using the default dbusUtils.SESSION_BUS for the other
    -- in this case they are the same.
    local sessionAddr = os.getenv("DBUS_SESSION_BUS_ADDRESS")
    if g_dbusBusName == sessionAddr then
        g_dbusBusName = dbusUtils.SESSION_BUS
    end

    if g_verbose > 0 then

        print()
        print("Version:                            ", string.format("%s",APP_VER))

        if g_dbusBusName ~= dbusUtils.SESSION_BUS then
            print("Bus Name:                           ", g_dbusBusName)
        end

        print("Running as Server:                  ", g_serverMode)
        print("Verbose:                            ", g_verbose)
        print("Garbage Collect After Reply:        ", g_forceGC)
        print("Timestamp Performance:              ", g_timestamp, (g_timestamp==true) and "(in "..g_timeUnits..")" or "")

        if g_serverMode == true then

            print()
            print("NOTE: Connect to Bus:", STRESS_BUSNAME)

        else -- client

            print("Test Async Server Reply:            ", g_asyncMode)
            print("Ingnore Reply from Request:         ", g_ignoreReply)
            print("Reply Using Error Interface:        ", g_errorIface)
            print("Create/Kill Client After Replies:   ", g_killClients)
            print("Payload Size:                       ", g_payloadSize)
            if g_iterCount == 0 then
               print("Iteration Count:                    ", "infinite")
            else
                print("Iteration Count:                   ", g_iterCount)
            end
            print("Interpacket Wait Interval:          ", g_waitInterval .. " (sec)")
            if g_ignoreReply == true and g_waitInterval == 0 then
                print("FLOOD MODE:                        ", "TRUE")
            end
        end
        print()
    end

    if bHelp == true then
        -- this way you can see what is parsed prior to help
        print()
        print(arg[0], string.format("%s",APP_VER))
        print(helpText)
        os.exit(1)
    end

end -- ParseArgs

-----------------------------------------------
-- ShutdownDbus
--
-- Attempts to cleanly shutdown D-Bus and
-- deallocate resources. The main event loop
-- should exit as well.
--
-----------------------------------------------
function ShutdownDbus()

    if g_dbusBus then
        local bRet, errStr = g_dbusBus.close()
        if bRet == false then
            print("ERROR: failed to destroy DBus bus => ", errStr)
        end
        g_dbusBus = nil
    end

    -- Bail out of the event loop
    dbusUtils.stopLoop()

end -- ShutdownDbus

----------------------------------------------------------------
--- Snapshot
---
--- Stats snapshot
----------------------------------------------------------------
function Snapshot(  )

    print("STATS:")
    print(string.rep("=", 40))
    pretty.dump( g_stats )

end -- Snapshot


----------------------------------------------------------------
--- main
----------------------------------------------------------------
local function main()

    ParseArgs()

    InitDbus()

    if g_serverMode ~= true then
        ClientRequests()
    else
        -- must be the server
        if g_verbose == 1 then
            io.stdout:write("Pkt: 0")
        end
    end

end -- main


--- Does not exit ---

dbusUtils.loop(main)





