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
 * @file           l2dbus_debug.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of the debug routines.
 *===========================================================================
 */
#include <string.h>
#include "l2dbus_debug.h"
#include "l2dbus_compat.h"
#include "lua.h"
#include "lauxlib.h"
#include "l2dbus_util.h"

#define L2DBUS_DUMP_FP    stderr


void
l2dbus_dumpItem
    (
    struct lua_State*   L,
    int                 idx,
    const char*         prefix
    )
{
    int t = lua_type(L, idx);
    prefix = (prefix != NULL) ? prefix : "";

    switch (t)
    {
        case LUA_TSTRING:
        {
            fprintf(L2DBUS_DUMP_FP, "%s'%s'", prefix, lua_tostring(L,idx));
            break;
        }

        case LUA_TBOOLEAN:
        {
            fprintf(L2DBUS_DUMP_FP, "%s%s", prefix, lua_toboolean(L,idx) ? "true" : "false");
            break;
        }

        case LUA_TNUMBER:
        {
            fprintf(L2DBUS_DUMP_FP, "%s" LUA_NUMBER_FMT, prefix, lua_tonumber(L,idx));
            break;
        }

        case LUA_TLIGHTUSERDATA:
        {
            fprintf(L2DBUS_DUMP_FP, "%slightuserdata", prefix);
            break;
        }

        default:
        {
            fprintf(L2DBUS_DUMP_FP, "%s%s", prefix, lua_typename(L, t));
            break;
        }
    }
}


void
l2dbus_dumpUserData
    (
    struct lua_State*   L,
    int                 udIdx,
    const char*         prefix
    )
{
    prefix = (prefix != NULL) ? prefix : "";

    if ( !lua_getmetatable(L, udIdx) )
    {
        fprintf(L2DBUS_DUMP_FP, "%suserdata has no metadata\n", prefix);
    }
    else
    {
        /* If there is a prefix to print then ... */
        if ( strcmp(prefix, "") )
        {
            fprintf(L2DBUS_DUMP_FP, "%s\n", prefix);
        }
        l2dbus_dumpTable(L, lua_gettop(L), "metatable");

        /* Pop off the metatable */
        lua_pop(L, 1);
    }
}


void
l2dbus_dumpTable
    (
    struct lua_State*   L,
    int                 tableIdx,
    const char*         name
    )
{
    /* Get the absolute value of the index in
     * case it's relative since we're going to
     * be pushing things on the stack.
     */
    tableIdx = lua_absindex(L, tableIdx);

    name = (name != NULL) ? name : "";
    if ( !lua_istable(L, tableIdx) )
    {
        fprintf(L2DBUS_DUMP_FP,
                "Item [%s] is not a table but %s\n",
                name, luaL_typename(L, tableIdx));
    }
    else
    {
        fprintf(L2DBUS_DUMP_FP,"Table: %s\n", name);
        lua_pushnil(L);
        while ( lua_next(L, tableIdx) != 0 )
        {
            l2dbus_dumpItem(L, -2, "\t[key]");
            l2dbus_dumpItem(L, -1, "\t[value]");
            /* Pop the value from the stack for the next go-around */
            lua_pop(L, 1);
            fprintf(L2DBUS_DUMP_FP, "\n");
        }
    }
}


void
l2dbus_dumpStack
    (
    struct lua_State*   L
    )
{
    int i;
    int top = lua_gettop(L);
    fprintf(L2DBUS_DUMP_FP, "Dumping Lua Stack (# Elts=%d)\n", top);
    for ( i = 1; i <= top; i++ )
    {
        l2dbus_dumpItem(L, i, NULL);
        if ( i < top )
        {
            fprintf(L2DBUS_DUMP_FP, " ");
        }
    }
    fprintf(L2DBUS_DUMP_FP, "\n");
}




