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
@file           validate.lua
@author         Glenn Schmottlach
@brief          Functions to validate/verify parameters.
*****************************************************************************
--]]

--- Validation Module.
-- This module contains several useful functions for validating arguments to
-- ensure they're the correct types or formatted according to the D-Bus
-- specification.
-- @module l2dbus.validate
-- @alias M

local MAX_DBUS_NAME_LEN = 255


local M = { }

--- Set to **true** to enable validation, **false** to skip the calls to
-- do validation or check argument types.
-- This provides a convenient global property that can turn real validation
-- on or off based on possible performance needs or debugging options. By
-- default validation is **enabled** (true).
M.doValidation = true

-- Dummy function that does not validation.
local function noValidation(...)
end


local function strip(text)
	return text:match("^%s*(.-)%s*$")
end


-- Extracts the tokens from a string separated by '|'.
-- Looks for words separated by the delimiter '|' and returns these
-- words in a Lua table structured as a "Set" container (e.g. words are the
-- keys, values set to **true**).
-- @tparam string text The text to extract the tokens from.
-- @treturn table A table containing the extracted tokens as keys of the table.
local function extractTokens(text)
	local tokens = {}
	local startIdx = 1
	local endIdx = string.find(text, "|");
	local value
	while endIdx do
		value = strip(string.sub(text, startIdx, endIdx - 1))
		if #value > 0 then
			tokens[value] = true
		end
		startIdx = endIdx + 1
		endIdx = string.find(text, "|", startIdx)
	end
	value = strip(string.sub(text, startIdx))
	if 0 < #value then
		tokens[value] = true
	end
	
	return tokens
end


--- Checks the argument types according to the specified pattern.
--  A pattern has the form of:
--  	type1|type2|...
--  Where type*N* is a Lua type or the '\*' wildcard character which matches
--  any type. The '|' character functions as a type name delimiter and
--  separates alternate types that can match (e.g. like a logical *OR*).
--  
--  @tparam string pattern The acceptable "types" pattern to match.
--  @tparam any ... The arguments to check against the pattern.
--  @treturn bool Returns **true** if the arguments match the pattern or
--  **false** if there is no match (e.g. the argument is not one of the
--  specified types).
--  @treturn nil|number If all the arguments match the acceptable types then
--  return *nil* otherwise return the argument index of the first argument that
--  doesn't match the type pattern.
function M.checkTypes(pattern, ...)
	local params = {...}
	local tokens = extractTokens(pattern)
	if tokens['*'] then
		return true, nil
	else	
	    for k = 1,#params do
	    	local typename = type(params[k])
	    	local altName
	    	if (("table" == typename) or ("userdata" == typename)) and
	    		("string" == type(params[k].__type)) then
	    		altName = params[k].__type
	    	end
	    	local isMatch = tokens[typename] or tokens[altName]
	    	if not isMatch then
	    		return false, k
	    	end
	    end
	end
    
    return true, nil
end

-- Verifies *v* is true and if not throw a Lua error with *msg* as the string.
-- @tparam bool v Value to verify whether it evaluates to **true** or **false*.
-- @tparam ?string msg The optional message to emit in the Lua error if *v*
-- evaluates to **false**.
-- @treturn nil	Returns **nil** if **v** evaluates to **true**, otherwise a
-- Lua error is thrown with **msg** as the error text.
local function doVerify(v, msg)
	if not v then
	    return error(msg and msg or "verification failure", 2)
	else
	    return nil
	end
end


-- Verify the arguments match the acceptable pattern of types.
--
-- @tparam string pattern Types pattern of the form:
-- 		type1|type2|typeN|...
-- @tparam any ... Argument list to test against the pattern.
-- @treturn nil Returns **nil** if all the arguments match the types pattern.
-- A Lua error is thrown if an unexpected type is found.
local function doVerifyTypes(pattern, ...)
	local result, idx = M.checkTypes(pattern, ...)
	if not result then
		return error("unexpected type for arg #" .. idx, 2)
	else
		return nil
	end
end


-- Check argument types and emit Lua error with message if invalid.
--
-- Similar to @{doVerifyTypes} except the Lua error that is emitted if
-- the types aren't valid with include the specified message.
--
-- @tparam string pattern Types pattern of the form:
-- 		type1|type2|typeN|...
-- @tparam string msg The message to include with any Lua error emitted.
-- @tparam any ... Argument list to test against the pattern.
-- @treturn nil Returns **nil** if all the arguments match the types pattern.
-- A Lua error is thrown if an unexpected type is found.
local function doVerifyTypesWithMsg(pattern, msg, ...)
	local result, idx = M.checkTypes(pattern, ...)
	if not result then
		return error(msg, 2)
	else
		return nil
	end
end


--- A simple UTF-8 validator in Lua.
-- Tested only with texlua. </br>
-- Manuel Pégourié-Gonnard, 2009, WTFPL v2.</br>
-- See <a href="http://www.wtfpl.net/about/">license</a> for details on WTFPL v2.
-- @tparam string str The string to check to see if it's valid UTF-8 text.
-- @treturn bool Returns true if *str* is a valid utf-8 sequence according
-- to RFC 3629.
function M.isValidUtf8(str)
	local len = string.len(str)
	local not_cont = function(b) return b == nil or b < 128 or b >= 192 end
	local i = 0
	local next_byte = function()
		i = i + 1
		return string.byte(str, i)
	end
	while i < len do
		local seq = {}
		seq[1] = next_byte()
		if seq[1] >= 245 then
			return false, 'Illegal byte '..seq[1]..' at byte '..i
		end
		if seq[1] >= 128 then
			local offset -- non-coding bits of the 1st byte
			for l, threshold in ipairs{[2] = 192, 224, 240} do
				if seq[1] >= threshold then -- >= l byte sequence
					seq[l] = next_byte()
					if not_cont(seq[l]) then
						return false, 'Illegal continuation byte '..
						seq[l]..' at byte '..i
					end
					offset = threshold
				end
			end
			if offset == nil then
				return false, 'Illegal first byte '..seq[1]..' at byte '..i
			end
			-- compute the code point for some verifications
			local code_point = seq[1] - offset
			for j = 2, #seq do
				code_point = code_point * 64 + seq[j] - 128
			end
			local n -- nominal length of the bytes sequence
			if code_point <= 0x00007F then n = 1
			elseif code_point <= 0x0007FF then n = 2
			elseif code_point <= 0x00FFFF then n = 3
			elseif code_point <= 0x10FFFF then n = 4
			end
			if n == nil then
				return false,
				'Code point '..code_point..' too large at byte '..i
			end
			if n ~= #seq then
				return false, 'Overlong encoding at byte '..i
			end
			if code_point >= 0xD800 and code_point <= 0xDFFF then
				return false, 'Code point '..code_point..
				' reserved for utf-16 surrogate pairs at byte '..i
			end
		end -- if seq[0] >= 128
	end
	return true
end


--- Verifies the given name is a valid D-Bus bus name.
-- 
-- Checks to see whether the provided name meets the
-- <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">requirements</a>
-- for a valid D-Bus bus name.
-- 
-- @tparam string name D-Bus bus name to validate.
-- @treturn bool Returns **true** if bus name is valid, **false** otherwise.
function M.isValidBusName(name)
    local isValid = true
    if type(name) ~= "string" then
        isValid = false
    else
        local busName = strip(name)
        if #busName > MAX_DBUS_NAME_LEN then
            isValid = false
        elseif #busName < 2 then
            isValid = false
        elseif not M.isValidUtf8(busName) then
        	isValid = false
        end
        
        -- If this is a "unique" bus name then ...
        if ":" == string.sub(busName, 1, 1) then
        	-- Skip the leading ":"
        	busName = busName:sub(2)
        -- Well-known names *cannot* start with a digit
        elseif string.find(busName, "^%d") ~= nil then
        	isValid = false
        end
        
        if isValid then
	        if string.find(busName, "[^A-Za-z0-9_%-.]") ~= nil then
	            isValid = false
	        elseif string.find(busName, "^.*%.%..*") ~= nil then
	            isValid = false -- prevent "com..example.foo"
	        elseif string.find(busName, "^[A-Za-z0-9_%-]+%.[A-Za-z0-9_%-]+") == nil then
	            isValid = false
	        end
	    end
    end

    return isValid

end -- isValidWellKnownBusName


--- Verifies the given name is a valid D-Bus object path.
-- 
-- Checks to see whether the provided path name meets the
-- <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">requirements</a>
-- for a valid D-Bus object path.
-- 
-- @tparam string name D-Bus object path to validate.
-- @treturn bool Returns **true** if object path is valid, **false** otherwise.
function M.isValidObjectPath(name)
    local isValid = true
    if type(name) ~= "string" then
        isValid = false
    else
        local objPath = strip(name)
        if #objPath == 0 then
            isValid = false
        elseif string.find(objPath, "[^A-Za-z0-9_/]") ~= nil then
            isValid = false
        elseif string.find(objPath, "^/[A-Za-z0-9_]*") == nil then
            isValid = false
        elseif string.sub(objPath,1,1) ~= '/' then
            isValid = false
        elseif (string.sub(objPath,#objPath,#objPath)) == '/' and
        	(#objPath ~= 1) then
            isValid = false
        elseif string.find(objPath, "//") ~= nil then
            isValid = false
        end
    end

    return isValid
end -- isValidObjectPath


--- Verifies the given name is a valid D-Bus member name.
-- 
-- Checks to see whether the provided name meets the
-- <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">requirements</a>
-- for a valid D-Bus member name. Member names are D-Bus member or signal
-- names.
-- 
-- @tparam string name D-Bus member name to validate.
-- @treturn bool Returns **true** if the member name is valid,
-- **false** otherwise.
function M.isValidMember(name)
    local isValid = true
    if type(name) ~= "string" then
        isValid = false
    else
        local member = strip(name)
        if #member > MAX_DBUS_NAME_LEN then
            isValid = false
        elseif #member == 0 then
            isValid = false
        elseif not M.isValidUtf8(member) then
        	isValid = false
        elseif string.find(member, "^%d") ~= nil then
        	isValid = false
        elseif string.find(member, "[^A-Za-z0-9_]") ~= nil then
            isValid = false
        end
    end
    return isValid
end


--- Verifies the given name is a valid D-Bus interface name.
-- 
-- Checks to see whether the provided name meets the
-- <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">requirements</a>
-- for a valid D-Bus interface name.
-- 
-- @tparam string name D-Bus interface name to validate.
-- @treturn bool Returns **true** if the interface name is valid,
-- **false** otherwise.
function M.isValidInterface(name)
    local isValid = true
    if type(name) ~= "string" then
        isValid = false
    else
        local infName = strip(name)
        if #infName > MAX_DBUS_NAME_LEN then
            isValid = false
        elseif #infName < 2 then
            isValid = false
        elseif not M.isValidUtf8(infName) then
        	isValid = false
        elseif string.find(infName, "^%d") ~= nil then
        	isValid = false
        elseif string.find(infName, "[^A-Za-z0-9_.]") ~= nil then
            isValid = false
        elseif string.find(infName, "^.*%.%..*") ~= nil then
            isValid = false -- prevent "com..example.foo"
        elseif string.find(infName, "^[A-Za-z0-9_]+%.[A-Za-z0-9_]+") == nil then
            isValid = false
        end

    end

    return isValid
end



--- Verifies the given name is a valid D-Bus error name.
-- Checks to see whether the provided name meets the
-- <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">requirements</a>
-- for a valid D-Bus error name. This requirements are identical to the
-- requirements for a D-Bus interface name.
-- @see isValidInterface
-- @tparam string name D-Bus error name to validate.
-- @treturn bool Returns **true** if the error name is valid,
-- **false** otherwise.
--- @function isValidErrorName
M.isValidErrorName = M.isValidInterface


--- Verifies *v* is true and if not throw a Lua error with *msg* as the string.
-- 
-- @tparam bool v Value to verify whether it evaluates to **true** or **false*.
-- @tparam ?string msg The optional message to emit in the Lua error if *v*
-- evaluates to **false**.
-- @treturn nil	Returns **nil** if **v** evaluates to **true**, otherwise a
-- Lua error is thrown with **msg** as the error text.
--- @function verify 
M.verify = M.doValidation and doVerify or noValidation

--- Verify the arguments match the acceptable pattern of types.
--
-- @tparam string pattern Types pattern of the form:
-- 		type1|type2|typeN|...
-- @tparam any ... Argument list to test against the pattern.
-- @treturn nil Returns **nil** if all the arguments match the types pattern.
-- A Lua error is thrown if an unexpected type is found.
--- @function verifyTypes
M.verifyTypes = M.doValidation and doVerifyTypes or noValidation


--- Check argument types and emit Lua error with message if invalid.
--
-- Similar to @{verifyTypes} except the Lua error that is emitted if
-- the types aren't valid with include the specified message.
--
-- @tparam string pattern Types pattern of the form:
-- 		type1|type2|typeN|...
-- @tparam string msg The message to include with any Lua error emitted.
-- @tparam any ... Argument list to test against the pattern.
-- @treturn nil Returns **nil** if all the arguments match the types pattern.
-- A Lua error is thrown if an unexpected type is found.
--- @function verifyTypesWithMsg
M.verifyTypesWithMsg = M.doValidation and doVerifyTypesWithMsg or noValidation


-- Called when this module is run as a program
local function main(arg)
    print("Module: " .. string.match(arg[0], "^(.+)%.lua"))
end

-- Determine the context in which the module is used
if require('is_main')() then
    -- The module is being run as a program
    main(arg)
else
    -- The module is being loaded rather than run
    return M
end
