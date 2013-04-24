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
 * @file           l2dbus_object.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of object registry.
 *===========================================================================
 */
#include <assert.h>
#include <string.h>
#include "lauxlib.h"
#include "l2dbus_object.h"
#include "l2dbus_compat.h"

static int gObjRegRef = LUA_NOREF;

void
l2dbus_objectRegistryNew
    (
    lua_State*  L
    )
{
    /* Create an object with a weak value that references object handles */
    lua_newtable(L);
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "v");
    lua_setfield(L, -2, "__mode");
    lua_setmetatable(L, -2);

    gObjRegRef = luaL_ref(L, LUA_REGISTRYINDEX);
}


int
l2dbus_objectRegistryCount
    (
    lua_State*  L
    )
{
    int objCount = 0;
    lua_rawgeti(L, LUA_REGISTRYINDEX, gObjRegRef);
    if ( lua_type(L, -1) != LUA_TTABLE )
    {
        luaL_error(L, "Object Registry not initialized!");
    }

    lua_pushnil(L);
    while ( lua_next(L, -2) != 0 )
    {
        objCount++;
        lua_pop(L, 1);
    }
    lua_pushinteger(L, objCount);

    return 1;
    return objCount;
}


void
l2dbus_objectRegistryAdd
    (
    lua_State*  L,
    void*       key,
    int         objIdx
    )
{
    /* Get the absolute index of the object */
    objIdx = lua_absindex(L, objIdx);
    lua_rawgeti(L, LUA_REGISTRYINDEX, gObjRegRef);
    if ( lua_type(L, -1) != LUA_TTABLE )
    {
        luaL_error(L, "Object Registry not initialized!");
    }

    /* Use the obj pointer (lightuserdata) as the key and
     * the object stack index as the value.
     */
    lua_pushlightuserdata(L, key);
    lua_pushvalue(L, objIdx);
    lua_rawset(L, -3);

    /* Pop the registry table off the stack */
    lua_pop(L, 1);
}

void*
l2dbus_objectRegistryGet
    (
    lua_State*  L,
    void*       key
    )
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, gObjRegRef);
    if ( lua_type(L, -1) != LUA_TTABLE )
    {
        luaL_error(L, "Object Registry not initialized!");
    }

    lua_pushlightuserdata(L, key);
    lua_rawget(L, -2);

    /* Remove the registry table and just leave either the value
     * associated with the key or nil.
     */
    lua_remove(L, -2);

    return lua_touserdata(L, -1);
}


void
l2dbus_objectRegistryRemove
    (
    lua_State*  L,
    void*       key
    )
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, gObjRegRef);
    if ( lua_type(L, -1) != LUA_TTABLE )
    {
        luaL_error(L, "Object Registry not initialized!");
    }

    lua_pushlightuserdata(L, key);
    lua_pushnil(L);
    lua_rawset(L, -3);

    /* Pop off the registry table */
    lua_pop(L, 1);
}


void*
l2dbus_objectNew
    (
    lua_State*      L,
    size_t          size,
    l2dbus_TypeId   typeId
    )
{
    void* object;
    const char* typeName = l2dbus_getNameByTypeId(typeId);

    assert( size >= 0);
    assert( NULL != typeName);

    object = lua_newuserdata(L, size);
    memset(object, 0, size);
    luaL_getmetatable(L, typeName);
    lua_setmetatable(L, -2);

#if 0
    /* Create an associated table to hold references to callbacks and
     * other such things.
     */
    lua_createtable(L, 1, 0);
    lua_setuservalue(L, -2);
#endif

    return object;
}

