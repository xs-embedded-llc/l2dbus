#-------------------------------------------------------------
# Simple script to setup the LUA_PATH for development/testing
#
# NOTE: MUST be run from the test root directory
#
# Usage: source ./setLuaEnv.sh
#        . ./setLuaEnv.sh 
#-------------------------------------------------------------

# Taylor this as needed...
#export LD_LIBRARY_PATH="/home/gschmottlach/workspace/cdbus/x86_64-linux-debug/"
#export LUA_PATH="${LUA_PATH};;/home/gschmottlach/workspace/l2dbus/?.lua;/home/gschmottlach/workspace/l2dbus/?/init.lua"
#export LUA_CPATH="${LUA_CPATH};;/home/gschmottlach/workspace/l2dbus/x86-64-debug/?.so"

echo ""
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "LUA_PATH=$LUA_PATH"
echo "LUA_CPATH=$LUA_CPATH"
echo ""

# Now run a simple Lua dependency check since 
# the core components rely on some modules/files.
if [ -e ./dependsCheck.lua ]; then
  lua ./dependsCheck.lua
fi
