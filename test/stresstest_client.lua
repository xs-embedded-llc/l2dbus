#!/usr/bin/env lua
local luaState = require("utils.luastate")
local ProxyController = require("l2dbus.proxyctrl")
local Prompter = require("utils.prompter")
local l2dbus = require("l2dbus")
local pretty = require("pl.pretty")
local app = require("pl.app")
local path = require("pl.path")
local ev = require("ev")

local APP_VER = "1.0.0"
local EXIT_SUCCESS = 0
local EXIT_FAILURE = 1

local L2DBUS_STRESS_TEST_SVC_BUS_NAME = "org.l2dbus.service.StressTest"
local L2DBUS_STRESS_TEST_SVC_OBJECT_PATH = "/org/l2dbus/StressTest"
local L2DBUS_STRESS_TEST_SVC_INTERFACE = "org.l2dbus.StressTest"

local gQuit = false
local gEvLoop =  nil
local gDispatcher = nil
local gPrompter = nil
local gFlags = nil
local gProxyCtrl = nil
local gPayload = ""
local gThreads = {}
local gOptions =
		{
		nonBlocking = true,
		bus = l2dbus.Dbus.BUS_SESSION,
		count = -1,
		replyWithError = false,
		interval = 0,
		noWait = false,
		rxSize = 64,
		txSize = 64,
		numThreads = 1,
		verbose = 1,
		replyDelay = 0
		}

local function log(level, fmt, ...)
	if level <= gOptions.verbose then
		io.stderr:write(string.format(fmt, ...) .. "\n")
	end
end


local function menuMain()
    while not gQuit do
        print("\nL2DBUS Stress Test Client v" .. APP_VER)
        print(string.rep("=", 50))
        print()
        print("Q.   Quit the stress test service")
        print("q.   Quit the stress test client")
        print(string.rep("=", 50))

        io.stdout:write("\nEnter a command: ")
        local cmd = gPrompter:getChar()
        print("\n")
        if cmd == 'q' then
            gQuit = true
        elseif cmd == 'Q' then
        	gProxyCtrl:getProxy(L2DBUS_STRESS_TEST_SVC_INTERFACE).m.Quit()
        end
    end
    
	local timeout = l2dbus.Timeout.new(gDispatcher, 250,
										true,
										function(tm, co)
											tm:setEnable(false)
											coroutine.resume(co)
										end,
										coroutine.running())
										    
    -- Wait for the coroutines to exit
    local numExited
    repeat
    	numExited = 0
    	for _,co in pairs(gThreads) do
	    	if coroutine.status(co) == "dead" then
	    		numExited = numExited + 1
	    	end
	    end
	    if numExited ~= #gThreads then
	    	timeout:setEnable(true)
	    	coroutine.yield()
	    	timeout:setEnable(false)
	    end
    until numExited == #gThreads
    
    gDispatcher:stop()
end


local function usage()
	print(string.format("\nClient Stress Test: Version %s", APP_VER))
	print(string.rep("-", 40))
	print(string.format("\nUsage: %s [options]", path.basename(arg[0])))
	print("\nOptions:")
	print("\t--block                Make calls to service blocking. Defaults")
	print("\t                       to non-blocking calls.")
	print("\t--bus [name]           The bus name to use. By default")
	print("\t                       the D-Bus Session bus is used.")
	print("\t-c [num]               The number of times for each thread to send")
	print("\t                       a message. Default=-1 (unlimited).")
	print("\t-e                     Server replies via error messages.")
	print("\t-h|--help              Shows this help.")
	print("\t-i [msec_interval]     The msec interval to wait between making calls.")
	print("\t                       (default=0 msec.)")
	print("\t-n|--nowait            Don't wait for replies. Default is to wait.")
	print("\t--rdelay               The msec delay in replies from far-end.")
	print("\t                       (default=0 msec.")
	print("\t--rxsize [nBytes]      The rx payload size (bytes).")
	print("\t                       (default=64)")
	print("\t--txsize [nBytes]      The tx payload size (bytes).")
	print("\t                       (default=64)")
	print("\t--threads [num]        The number of coroutine based threads")
	print("\t                       to concurrently send messages. Default=1.")
	print("\t-v                     Turns on verbose output")
end


local function executeCall(proxy, count)
	local callSuccess = false
	local msg
	
	if gOptions.nonBlocking then
		local result, pending, errMsg = proxy.m.Test(count, gPayload)
		if not result then
			log(1,  "Test %d failed to execute: %s - %s", count, pending, errMsg)			
		elseif gOptions.noWait then
			callSuccess = true
		else
			local c = coroutine.running()
			pending:setNotify(function(p, co)
					local m = p:stealReply()
					return coroutine.resume(co, m)
				end, 
				c)
			if pending:isCompleted() then
				msg = pending:stealReply()
			else
				msg = coroutine.yield()
				--if not msg then os.exit(EXIT_FAILURE) end
			end

			if nil == msg then
				log(3, "\n**** nil message received for reply")
			elseif msg:getType() == l2dbus.Dbus.MESSAGE_TYPE_ERROR then
				if gOptions.replyWithError then
					local seq, payload = msg:getArgs()
					if #payload ~= gOptions.rxSize then
						log(1, "Test %d failed: expected %d bytes received %d bytes", count,
							gOptions.rxSize, #payload)
					elseif seq ~= count then
						log(1, "Test %d failed: expected seq %d received %d",
							count, count, seq)
					else
						callSuccess = true
					end
					
				else
					log(1, "Test %d failed - unexpected error: %s - %s", count,
							msg:getErrorName(), tostring(msg:getArgs()))
				end
			-- Else must be a normal reply message
			else
				if gOptions.replyWithError then
					log(1, "Test %d failed: expected to receive an error reply", count)
				else
					local seq, payload = msg:getArgs()
					if #payload ~= gOptions.rxSize then
						log(1, "Test %d failed: expected %d bytes received %d bytes", count,
							gOptions.rxSize, #payload)
					elseif seq ~= count then
						log(1, "Test %d failed: expected seq %d received %d", count,
							count, seq)
					else
						callSuccess = true
					end					
				end
			end
		end
	else
		local result, seq, payload = proxy.m.Test(count, gPayload)
		if not result then
			if gOptions.replyWithError then
				if #payload ~= gOptions.rxSize then
					log(1, "Test %d failed: expected %d bytes received %d bytes", count,
						gOptions.rxSize, #payload)
				else
					callSuccess = true
				end 
			else
				log(1, "Test %d failed to execute: %s - %s", count,
					seq, payload)
			end
		-- If we're not waiting on the result				
		elseif gOptions.noWait then
			callSuccess = true
		else
			if #payload ~= gOptions.rxSize then
				log(1, "Test %d failed: expected %d bytes received %d bytes", count,
					gOptions.rxSize, #payload)
			else
				callSuccess = true
			end
		end
	end
	
	log(3, "\nExiting executeCall")
	return callSuccess
end


local function runTest(conn, id)
	local proxyCtrl = ProxyController.new(conn, L2DBUS_STRESS_TEST_SVC_BUS_NAME,
										L2DBUS_STRESS_TEST_SVC_OBJECT_PATH)
	assert(proxyCtrl:bind())
	local interval = (gOptions.interval > 0) and gOptions.interval or 1
	local timeout = l2dbus.Timeout.new(gDispatcher, interval,
										true,
										function(tm, co)
											tm:setEnable(false)
											coroutine.resume(co)
										end,
										coroutine.running())
	
	proxyCtrl:setProxyNoReplyNeeded(gOptions.noWait == true)
	proxyCtrl:setBlockingMode(gOptions.nonBlocking == false)
	
	local proxy = proxyCtrl:getProxy(L2DBUS_STRESS_TEST_SVC_INTERFACE)
	local result
	local count = 1
	if gOptions.count > 0 then
		while not qQuit and count <= gOptions.count do
			log(2,"\nThread #" .. id .. " executing")
			result = executeCall(proxy, count)
			if gOptions.verbose > 0 then
				if result then
					io.stderr:write(string.format("\n[%d] Test %d: Success", id, count))
				else
					io.stderr:write(string.format("\n[%d] Test %d: Failure", id, count))
				end
			end
			count = count + 1
			
			timeout:setEnable(true)
			coroutine.yield()	
		end
	else
		while not gQuit do
			log(2, "\nThread #" .. id .. " executing")
			result = executeCall(proxy, count)
			if gOptions.verbose > 0 then
				if result then
					io.stderr:write(string.format("\n[%d] Test %d: Success", id, count))
				else
					io.stderr:write(string.format("\n[%d] Test %d: Failure", id, count))
				end
			end
			count = count + 1
			
			timeout:setEnable(true)
			coroutine.yield()	
		end
	end	
end


local function setup(timeout, conn)
	local co = coroutine.create(menuMain)
    local status, result = coroutine.resume(co)
    if not status then
        log(1, "\nError: " .. result)
    end	
	-- Kick off several coroutines to talk to the stress tester service
	for i = 1, gOptions.numThreads do
		co = coroutine.create(runTest)
		gThreads[i] = co
		status, result = coroutine.resume(co, conn, i)
		if not status then
			log(1, "\nError: " .. result)
		end
	end	
end

local function cleanup()
	gPrompter:restoreTtyState()
	gPrompter = nil
	gProxyCtrl = nil
	gDispatcher = nil
end


local function main()
	local conn
	local flags, appArgs = app.parse_args(nil, {})
	
	for flag, value in pairs(flags) do
		if flag == "block" then
			gOptions.nonBlocking = false
		elseif flag == "bus" then
			gOptions.bus = value
		elseif flag == "c" then
			gOptions.count = tonumber(value)
		elseif flag == "e" then
			gOptions.replyWithError = true
		elseif flag == "h" or flag == "help" then
			usage()
			os.exit(EXIT_SUCCESS)
		elseif flag == "i" then
			gOptions.interval = tonumber(value)
		elseif flag == "n" or flag == "nowait" then
			gOptions.noWait = true
		elseif flag == "rdelay" then
			gOptions.replyDelay = tonumber(value)
		elseif flag == "rxsize" then
			gOptions.rxSize = tonumber(value)
		elseif flag == "txsize" then
			gOptions.txSize = tonumber(value)
			if gOptions.txSize >= 0 then
				gPayload = string.rep('C', gOptions.txSize)
			else
				io.stderr:write("TX size must be >= 0\n")
				os.exit(EXIT_FAILURE)
			end
		elseif flag == "threads" then
			gOptions.numThreads = tonumber(value)
		elseif flag == "v" then
			gOptions.verbose = tonumber(value)
		end
	end
	
	print("Configuration:")
	pretty.dump(gOptions)
	
	if gOptions.verbose > 4 then
		l2dbus.Trace.setFlags(l2dbus.Trace.ALL)
	else
		l2dbus.Trace.setFlags(l2dbus.Trace.ERROR, l2dbus.Trace.WARN)
	end
	
	gEvLoop = ev.Loop.default
	-- Force the loop to be instantiated by calling a method on it
	gEvLoop:now()
	gPrompter = Prompter:new(gEvLoop)
	local mainLoop = require("l2dbus_ev").MainLoop.new(gEvLoop)
	gDispatcher = l2dbus.Dispatcher.new(mainLoop)
	
	if type(gOptions.bus) == "number" then
		conn = l2dbus.Connection.openStandard(gDispatcher,
												gOptions.bus, false, false)
	else
		conn = l2dbus.Connection.open(gDispatcher, gOptions.bus, false, false)
	end
	
	assert( nil ~= conn )

	if gOptions.verbose > 2 then
		print(string.format("Connection: max message size: %d (bytes)",
				conn:getMaxMessageSize()))
		print(string.format("Connection: max received size: %d (bytes)",
				conn:getMaxReceivedSize()))
	end
	
	gProxyCtrl = ProxyController.new(conn, L2DBUS_STRESS_TEST_SVC_BUS_NAME,
										L2DBUS_STRESS_TEST_SVC_OBJECT_PATH)
	gProxyCtrl:setBlockingMode(true)
	assert(gProxyCtrl:bind())
	
	local proxy = gProxyCtrl:getProxy(L2DBUS_STRESS_TEST_SVC_INTERFACE)
	local status, result = proxy.m.SetOptions({payloadSize = gOptions.rxSize,
						replyWithError = gOptions.replyWithError,
						replyDelay = gOptions.replyDelay})
	if not status then
		io.stderr:write("Failed to configure the service with options: " .. result)
		os.exit(EXIT_FAILURE)
	end
    	
	-- Set up a timer to kick-off the clients immediately	
	local timeout = l2dbus.Timeout.new(gDispatcher, 1, false, setup, conn)
	timeout:setEnable(true)
	
	-- Returns when command to quit
	gDispatcher:run(l2dbus.Dispatcher.DISPATCH_WAIT)
end

print("Hit Return to continue")
io.stdin:read("*l")
print("Starting program")

main()
cleanup()
l2dbus.shutdown()
collectgarbage("collect")

print("Dump after nil'ing out everything")
luaState.dump_stats(io.stdout)