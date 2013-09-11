
local function is_main()
    return debug.getinfo(4) == nil
end

return is_main
