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
 * @file           l2dbus_util.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of common utility types, functions, etc...
 *******************************************************************************
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
    return NULL ; /* not what is wanted */
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
    return loop ; /* not what is wanted */
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

    luaL_error(L, "%s : %s/%s/0x%X", msg, sev, fac, code);
}


int
l2dbus_createMetatable
    (
    lua_State*      L,
    l2dbus_TypeId   typeId,
    const luaL_Reg* funcs
    )
{
    if ( luaL_newmetatable(L, l2dbus_getNameByTypeId(typeId)) )
    {
        /* Assign the methods to this new metatable */
        luaL_setfuncs(L, funcs, 0);

        lua_pushinteger(L, typeId);
        lua_setfield(L, -2, L2DBUS_META_TYPE_ID_FIELD);

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
