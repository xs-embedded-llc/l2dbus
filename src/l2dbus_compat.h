/*******************************************************************************
 * 
 * Project         l2dbus
 * (c) Copyright   2012 XS-Embedded LLC
 *                 All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *******************************************************************************
 *******************************************************************************
 * @file           l2dbus_compat.h
 * @author         Glenn Schmottlach
 * @brief          Lua version compatibility layer definitions.
 *******************************************************************************
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
