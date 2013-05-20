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
 * @file           l2dbus_util.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of common utility types, functions, etc...
 *===========================================================================
 */
#include <stdlib.h>
#include "l2dbus_util.h"
#include "lauxlib.h"
#include "l2dbus_debug.h"
#include "l2dbus_compat.h"
#include "l2dbus_defs.h"
#include "ev.h"

/* Lua Libev metatable type name. This would likely
 * break should the implementation of the Lua binding
 * to libev change.
 */
const char* const L2DBUS_LOOP_MT = "ev{loop}";


void*
l2dbus_isUserData
    (
    lua_State*  L,
    int         udIdx,
    const char* typeName
    )
{
    /* Is value a userdata? */
    void *p = lua_touserdata(L, udIdx);
    if ( NULL != p )
    {
        /* Does it have a metatable? */
        if ( lua_getmetatable(L, udIdx) && (NULL != typeName) )
        {
            lua_getfield(L, LUA_REGISTRYINDEX, typeName);
            /* Does it have the correct mt? */
            if ( !lua_rawequal(L, -1, -2) )
            {
                p = NULL;
            }
            lua_pop(L, 2);
            return p;
        }
    }
    return NULL;
}

struct ev_loop*
l2dbus_isEvLoop
    (
    lua_State*  L,
    int         udIdx
    )
{
    /* Is value a userdata? */
    struct ev_loop* loop = NULL;
    struct ev_loop** p = lua_touserdata(L, udIdx);
    if ( NULL != p )
    {
        /* Does it have a metatable? */
        if ( lua_getmetatable(L, udIdx) )
        {
            lua_getfield(L, LUA_REGISTRYINDEX, L2DBUS_LOOP_MT);
            /* Does it have the correct mt? */
            if ( lua_rawequal(L, -1, -2) )
            {
                loop = *p;
            }
            lua_pop(L, 2);
        }
    }
    return loop;
}

void
l2dbus_cdbusError
    (
    lua_State*      L,
    cdbus_HResult   rc,
    const char*     msg
    )
{
    const char* fac = "UNK";
    unsigned code = CDBUS_ERR_CODE(rc);
    const char* sev = CDBUS_FAILED(rc) ? "FAIL": "PASS";
    msg = (msg == NULL) ? "" : msg;

    switch ( CDBUS_FACILITY(rc) )
    {
        case CDBUS_FAC_CDBUS:
            fac = "CDBUS";
            break;

        case CDBUS_FAC_DBUS:
            fac = "DBUS";
            break;

        case CDBUS_FAC_EV:
            fac = "LIBEV";
            break;

        default:
            break;
    }

    luaL_error(L, "%s : %s/%s/%p", msg, sev, fac, code);
}


int
l2dbus_createMetatable
    (
    lua_State*      L,
    l2dbus_TypeId   typeId,
    const luaL_Reg* funcs
    )
{
    const char* typename = l2dbus_getNameByTypeId(typeId);
    if ( luaL_newmetatable(L, typename) )
    {
        /* Assign the methods to this new metatable */
        luaL_setfuncs(L, funcs, 0);

        lua_pushinteger(L, typeId);
        lua_setfield(L, -2, L2DBUS_META_TYPE_ID_FIELD);

        lua_pushstring(L, typename);
        lua_setfield(L, -2, L2DBUS_META_TYPE_NAME_FIELD);

        /* Set it's metatable to point to itself */
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    }

    return 1;
}


l2dbus_Bool
l2dbus_isValidIndex
    (
    lua_State*  L,
    int         idx
    )
{
    int top = lua_gettop(L);
    int isValid = L2DBUS_FALSE;

    if ( (1 <= abs(idx)) && (abs(idx) <= top) )
    {
        isValid = L2DBUS_TRUE;
    }

    return isValid;
}


l2dbus_TypeId
l2dbus_getMetaTypeId
    (
    lua_State*  L,
    int         idx
    )
{
    l2dbus_TypeId typeId = L2DBUS_INVALID_TYPE_ID;

    if ( LUA_TUSERDATA == lua_type(L, idx) )
    {
        lua_getfield(L, idx, L2DBUS_META_TYPE_ID_FIELD);
        if ( LUA_TNUMBER == lua_type(L, -1) )
        {
            typeId = (l2dbus_TypeId)lua_tointeger(L, -1);
            /* Make sure the type ID falls into the valid range */
            if ( (L2DBUS_START_TYPE_ID >= typeId) || (L2DBUS_END_TYPE_ID <= typeId) )
            {
                typeId = L2DBUS_INVALID_TYPE_ID;
            }
        }
        lua_pop(L, 1);
    }
    return typeId;
}


const char*
l2dbus_getTypeName
    (
    lua_State*  L,
    int         idx
    )
{
    const char* name = "unknown";

    l2dbus_TypeId typeId = l2dbus_getMetaTypeId(L, idx);
    if ( L2DBUS_INVALID_TYPE_ID == typeId )
    {
        name = lua_typename(L, idx);
    }
    else
    {
        name = l2dbus_getNameByTypeId(typeId);
    }

    return name;
}


void
l2dbus_getGlobalField
    (
    lua_State*  L,
    const char* name
    )
{
    lua_pushglobaltable(L);
    lua_getfield(L, -1, name);
    lua_remove(L, -2);
}


l2dbus_Bool
l2dbus_isString
    (
    lua_State*  L,
    int         nArg
    )
{
    /* Returns 'true' only if the argument is a string and not necessarily
     * convertable to a string.
     */
    return lua_type(L, nArg) == LUA_TSTRING;
}


const char*
l2dbus_checkString
    (
    lua_State*  L,
    int         nArg
    )
{
    lua_Debug dbgRec;
    const char* funcName = "unknown";

    if ( !l2dbus_isString(L, nArg) )
    {
        if ( 0 < lua_getstack(L, 0, &dbgRec) )
        {
            if ( 0 < lua_getinfo(L, "n", &dbgRec) )
            {
                funcName = dbgRec.name;
            }
        }

        luaL_error(L, "bad argument #%d to '%s' (string expected, got %s)",
                    lua_absindex(L, nArg),
                    funcName,
                    lua_typename(L, lua_type(L, nArg)));
    }

    return lua_tostring(L, nArg);
}

