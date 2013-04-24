/*===========================================================================
 * 
 * Project         l2dbus
 *
 * Released under the MIT License (MIT)
 * Copyright (c) 2013 XS-Embedded LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *===========================================================================
 *===========================================================================
 * @file           l2dbus_compat.h
 * @author         Glenn Schmottlach
 * @brief          Lua version compatibility layer definitions.
 *===========================================================================
 */

#ifndef L2DBUS_COMPAT_H_
#define L2DBUS_COMPAT_H_
#include "lua.h"
#include "lauxlib.h"

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM==501
void luaL_setfuncs(lua_State* L, const luaL_Reg* l, int nup);

#define luaL_newlibtable(L,l)   \
                    lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l) \
                    (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

#define lua_absindex(L, i)  \
                    ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? \
                    (i) : lua_gettop(L) + (i) + 1)

#define lua_setuservalue  lua_setfenv
#define lua_getuservalue  lua_getfenv
#define lua_pushglobaltable(L)  lua_pushvalue(L, LUA_GLOBALSINDEX)
#define lua_rawlen        lua_objlen


#define luaL_checkversion(L)    ((void)0)
#define luaL_reg luaL_Reg

#endif

#define lua_boxpointer(L,u) \
    (*(void **)(lua_newuserdata(L, sizeof(void *))) = (u))

#endif /* Guard for L2DBUS_COMPAT_H_ */
