#-------------------------------------------------------------
# Simple script to setup the LUA_PATH for development/testing
#
# NOTE: MUST be run from the test root directory
#
# Usage: source ./setLuaEnv.sh
#        . ./setLuaEnv.sh 
#-------------------------------------------------------------

# Tailor as needed for your build environment
export BUILD_ROOT="${HOME}/workspace"
export LD_LIBRARY_PATH="${BUILD_ROOT}/cdbus/x86_64-Debug/lib"
export LUA_PATH="${LUA_PATH};;${BUILD_ROOT}/l2dbus/lua/?.lua;${BUILD_ROOT}/l2dbus/lua/?/init.lua"
export LUA_CPATH="${LUA_CPATH};;${BUILD_ROOT}/l2dbus/x86_64-Debug/lib/?.so"

echo ""
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
echo "LUA_PATH=$LUA_PATH"
echo "LUA_CPATH=$LUA_CPATH"
echo ""

# Now run a simple Lua dependency check since 
# the core components rely on some modules/files.
if [ -e ./utils/dependsCheck.lua ]; then
  lua ./utils/dependsCheck.lua
fi
