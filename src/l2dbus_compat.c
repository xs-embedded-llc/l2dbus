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
 * @file           l2dbus_compat.c
 * @author         Glenn Schmottlach
 * @brief          Lua version compatibility layer implementation.
 *******************************************************************************
 */
#include <stdlib.h>
#include "l2dbus_compat.h"

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM==501
/*
** Adapted from Lua 5.2.0
*/
void luaL_setfuncs
    (
    lua_State*      L,
    const luaL_Reg* l,
    int             nup
    )
{
    luaL_checkstack(L, nup, "too many upvalues");
    for (; l->name != NULL ; l++)
    { /* fill the table with given functions */
        int i;
        for (i = 0; i < nup; i++) /* copy upvalues to the top */
            lua_pushvalue(L, -nup);
        lua_pushstring(L, l->name);
        lua_pushcclosure(L, l->func, nup); /* closure with those upvalues */
        lua_settable(L, -(nup + 3));
    }
    lua_pop(L, nup);
    /* remove upvalues */
}

#endif


