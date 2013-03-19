--[[
*****************************************************************************
Project         l2dbus
(c) Copyright   2013 XS-Embedded LLC
                 All rights reserved

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

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


local function doVerify(v, msg)
	if not v then
	    return error(msg, 2)
	else
	    return nil
	end
end


local function doVerifyTypes(pattern, ...)
	local result, idx = M.checkTypes(pattern, ...)
	if not result then
		return error("unexpected type for arg #" .. idx, 2)
	else
		return nil
	end
end


local function doVerifyTypesWithMsg(pattern, msg, ...)
	local result, idx = M.checkTypes(pattern, ...)
	if not result then
		return error(msg, 2)
	else
		return nil
	end
end


--- A simple UTF-8 validator in Lua.
-- Tested only with texlua.
-- Manuel Pégourié-Gonnard, 2009, WTFPL v2.
-- See <a href="http://www.wtfpl.net/about/">license</a> for details on WTFPL v2.
-- @tparam string str The string to check to see if it's valid UTF-8 text.
-- @treturn bool Returns true if *str* is a valid utf-8 sequence according
-- to rfc3629.
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
-- @tparam string name bus name to check
-- @treturn boolean True if bus name is valid, false otherwise.
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
-- @tparam string name The object path to check
-- @treturn boolean True if the object name is valid, false otherwise.
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
        	(#objPath == 1) then
            isValid = false
        elseif string.find(objPath, "//") ~= nil then
            isValid = false
        end
    end

    return isValid
end -- isValidObjectPath


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

M.isValidErrorName = M.isValidInterface

M.verify = M.doValidation and doVerify or noValidation
M.verifyTypes = M.doValidation and doVerifyTypes or noValidation
M.verifyTypesWithMsg = M.doValidation and doVerifyTypesWithMsg or noValidation


-- Called when this module is run as a program
local function main(arg)
    print("Module: " .. string.match(arg[0], "^(.+)%.lua"))
end

-- Determine the context in which the module is used
if type(package.loaded[(...)]) ~= "userdata" then
    -- The module is being run as a program
    main(arg)
else
    -- The module is being loaded rather than run
    return M
end