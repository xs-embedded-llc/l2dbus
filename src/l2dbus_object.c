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
 * @file           l2dbus_object.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of object registry.
 *******************************************************************************
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

