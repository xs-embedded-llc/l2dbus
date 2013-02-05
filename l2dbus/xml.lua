--[[
Copyright (C) 2009 Steve Donovan, David Manura.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
OR OTHER DEALINGS IN THE SOFTWARE.
]]

--[[
Original classic (software-only) XML parser by Roberto Ierusalimschy.
Modifications to parser by Steve Donovan with additional helper functions
taken from Penlight's XML module:
https://github.com/stevedonovan/Penlight/blob/master/lua/pl/xml.lua
]]


local t_insert = table.insert;
local t_remove = table.remove;
local tostring = tostring;
local setmetatable = setmetatable;
local getmetatable = getmetatable;
local type = type;
local s_find = string.find;


local M = {}
local Doc = { __type = "doc" };
Doc.__index = Doc;

local html_empty_elements = { --lists all HTML empty (void) elements
	br = true,
	img = true,
	meta = true,
	META = true,
	frame = true,
	area = true,
	hr = true,
	base = true,
	col = true,
	link = true,
	input = true,
	option = true,
	param = true,
}

local escapes = { quot = "\"", apos = "'", lt = "<", gt = ">", amp = "&" }
local function unescape(str) return (str:gsub( "&(%a+);", escapes)); end

local function parseargs(s)
	local html = M.parsehtml
	local arg = {}
	s:gsub("([%w:]+)%s*=%s*([\"'])(.-)%2", function (w, _, a)
		if html then w = w:lower() end
		arg[w] = unescape(a)
	end)
	if html then
		s:gsub("([%w:]+)%s*=%s*([^\"']+)%s*", function (w, a)
			w = w:lower()
			arg[w] = unescape(a)
		end)
	end
	return arg
end

function Doc:childtags()
	local i = 0;
	return function (a)
		local v
		repeat
			i = i + 1
			v = self[i]
			if v and type(v) == 'table' then return v; end
		until not v
	end, self[1], i;
end

function M.parse(s, all_text)
	local html = M.parsehtml
	local t_insert,t_remove = table.insert,table.remove
	local s_find,s_sub = string.find,string.sub
	local stack = {}
	local top = {}
	t_insert(stack, top)
	local ni,c,label,xarg, empty
	local i, j = 1, 1
	-- we're not interested in <?xml version="1.0"?>
	local _,istart = s_find(s,'^%s*<%?[^%?]+%?>%s*')
	if istart then i = istart+1 end
	while true do
		ni,j,c,label,xarg, empty = s_find(s, "<(%/?)([%w:%-_]+)(.-)(%/?)>", i)
		if not ni then break end
		local text = s_sub(s, i, ni-1)
		if html then
			label = label:lower()
			if html_empty_elements[label] then empty = "/" end
		end
		if all_text or not s_find(text, "^%s*$") then
			t_insert(top, unescape(text))
		end
		if empty == "/" then -- empty element tag
			t_insert(top, setmetatable({tag=label, attr=parseargs(xarg), empty=1},Doc))
		elseif c == "" then -- start tag
			top = setmetatable({tag=label, attr=parseargs(xarg)},Doc)
			t_insert(stack, top) -- new level
		else -- end tag
			local toclose = t_remove(stack) -- remove top
			top = stack[#stack]
			if #stack < 1 then
				error("nothing to close with "..label)
			end
			if toclose.tag ~= label then
				error("trying to close "..toclose.tag.." with "..label)
			end
			t_insert(top, toclose)
		end
		i = j+1
	end
	local text = s_sub(s, i)
	if all_text or not s_find(text, "^%s*$") then
		t_insert(stack[#stack], unescape(text))
	end
	if #stack > 1 then
		error("unclosed "..stack[#stack].tag)
	end
	local res = stack[1]
	return type(res[1])=='string' and res[2] or res[1]
end

return M