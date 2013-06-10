local M     = {} -- external API/members

-- Turn off all the debug information
M.logDebug = function(verbose, logLevel, fmt, ...) end


----------------------------------------------------------------
--- deepcopy
---
--- This function recursively copies a table's contents, and
--  ensures that metatables are preserved                              <BR>
---                                                                    <BR>
--- Code taken from:
--- <a href="http://snippets.luacode.org/snippets/Deep_copy_of_a_Lua_Table_2">Here</a> <BR>
---                                                                    <BR>
--- LICENSE: MIT/X11                                                   <BR>
---
--- @tparam   (string)  t ....table to copy
---
--- @treturn  (table) copied table
----------------------------------------------------------------
function M.deepcopy(t)
    if type(t) ~= 'table' then return t end
    local mt = getmetatable(t)
    local res = {}
    for k,v in pairs(t) do
        if type(v) == 'table' then
            v = M.deepcopy(v)
        end
        res[k] = v
    end
    setmetatable(res,mt)
    return res
end -- deepcopy



----------------------------------------------------------------
--- getopt
---
--- Same functionality as "getopt" and "getopt_long"
--- in BSD/Unix.                                                       <BR>
---                                                                    <BR>
--- "getopt() translated from the BSD
--- getopt(); compatible with the default
--- Unix getopt()"                                                     <BR>
---                                                                    <BR>
--- getopt_long() is meant to match the BSD
--- implementation.                                                    <BR>
---                                                                    <BR>
---************************************************                    <BR>
--- IMPORTANT: This call will remove entries
---            from the args list passed in.
---            If this is a problem make a
---            deepcopy() of the args before
---            calling this routine.                                   <BR>
---************************************************                    <BR>
---                                                                    <BR>
--- Code taken from:                                                   <BR>
--- <a href="https://github.com/attractivechaos/klib/blob/master/lua/klib.lua">Here</a>   <BR>
---                                                                    <BR>
--- Then added:                                                        <BR>
--- + long argument parsing on our own                                 <BR>
--- + pass back unknown arguments to continue the parsing process      <BR>
---   or allow you to terminate parsing--more flexible.
--- + Added debug parameter for better debugging                       <BR>
---                                                                    <BR>
--- LICENSE: MIT                                                       <BR>
---                                                                    <pre>
---  short example:
--- ----------------
---  for opt, val in os.getopt(args, 'a:b') do
---      print(opt, val)
---  end
---
---  long example:
--- ---------------
---  local args = { "--append", "--create", "cArg",
---                 "-b", "-a", "arg1",
---                 "--file", "fArg"  }
---
---  local long_options = {
---              --name      hasArg   short/retOpt
---              {"append",  false,   nil },
---              {"delete",  true,    'd' },
---              {"verbose", false,   'v' },
---              {"create",  true,    'c' },
---              {"file",    true,    'f' }   }
---  -- name         = long argument name
---  -- hasArg       = true, next arg in args is returned
---                    otherwise, nothing goes with this arg
---  -- short/retOpt = if non-nil this is returned instead of
---                    the "name" field.
---
---  for opt, val in os.getopt(args, 'a:b', long_options) do
---      print(opt, val)
---  end
---
---  OUTPUT of examnple:
---  append  nil
---  c       cArg
---  b       nil
---  a       arg1
---  f       fArg                                                 </pre>
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
function M.getopt(args, ostr, long_opt, bDebug)
    local arg, place = nil, 0;
    local iDebug     = (bDebug == true) and 1 or 0
    return function ()
        if place == 0 then -- update scanning pointer
            place = 1
            if #args == 0 or args[1]:sub(1, 1) ~= '-' then
                place = 0;

                if #args == 0 then
                    M.logDebug(iDebug,1, "getopt: nil (shortopt): end of args")
                    return nil
                else
                    local badArg = table.remove(args, 1)
                    M.logDebug(iDebug,1, "getopt: ? (shortopt): %s", badArg)
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
                        M.logDebug(iDebug,1, "getopt: ? (longopt): %s", longArg)
                        return "?", longArg
                    else
                        M.logDebug(iDebug,1, "getopt: retOpt/optVal: %s/%s", retOpt, optVal)
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
                M.logDebug(iDebug,1, "getopt: nil (unknown option)")
                return nil
            end
            if place > #args[1] then
                table.remove(args, 1);
                place = 0;
            end
            M.logDebug(iDebug,1, "getopt: ? (unknown option): %s", optopt)
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
                        M.logDebug(iDebug,1, "getopt: :")
                        return ':'
                    end
                    M.logDebug(iDebug,1, "getopt: ? (needs arg)")
                    return '?';
                else arg = args[1] end
            end
            table.remove(args, 1);
            place = 0;
        end
        M.logDebug(iDebug,1, "getopt: optopt/arg: %s/%s", optopt, arg)
        return optopt, arg;
    end

end -- getopt


return M

