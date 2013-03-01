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
 * @file           l2dbus_callback.c        
 * @author         Glenn Schmottlach
 * @brief          Callback implementation details
 *******************************************************************************
 */
#include <stdlib.h>
#include <assert.h>
#include "l2dbus_compat.h"
#include "l2dbus_callback.h"
#include "l2dbus_debug.h"
#include "lauxlib.h"

/* The Lua thread used to run all callbacks */
static lua_State*  gCallbackThread = NULL;
static int gCallbackRef = LUA_NOREF;

void
l2dbus_callbackConfigure
    (
    lua_State*   L
    )
{
    if ( NULL == gCallbackThread )
    {
        gCallbackThread = lua_newthread(L);
        gCallbackRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
}


void
l2dbus_callbackShutdown
    (
    lua_State*   L
    )
{
    if ( LUA_NOREF != gCallbackRef )
    {
        luaL_unref(L, LUA_REGISTRYINDEX, gCallbackRef);
        gCallbackRef = LUA_NOREF;
    }
}


lua_State*
l2dbus_callbackGetThread(void)
{
    return gCallbackThread;
}


void
l2dbus_callbackInit
    (
    l2dbus_CallbackCtx* ctx
    )
{
    assert( NULL != ctx );
    ctx->funcRef = LUA_NOREF;
    ctx->userRef = LUA_NOREF;
}


void
l2dbus_callbackRef
    (
    lua_State*          L,
    int                 funcIdx,
    int                 userIdx,
    l2dbus_CallbackCtx* ctx
    )
{
    if ( funcIdx != L2DBUS_CALLBACK_NOREF_NEEDED )
    {
        funcIdx = lua_absindex(L, funcIdx);
    }

    if ( userIdx != L2DBUS_CALLBACK_NOREF_NEEDED )
    {
        userIdx = lua_absindex(L, userIdx);
    }


    if ( L2DBUS_CALLBACK_NOREF_NEEDED != funcIdx )
    {
        lua_pushvalue(L, funcIdx);
        ctx->funcRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        ctx->funcRef = LUA_NOREF;
    }

    if ( L2DBUS_CALLBACK_NOREF_NEEDED != userIdx )
    {
        lua_pushvalue(L, userIdx);
        ctx->userRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        ctx->userRef = LUA_NOREF;
    }
}


void
l2dbus_callbackUnref
    (
    lua_State*          L,
    l2dbus_CallbackCtx* ctx
    )
{
    assert( NULL != ctx );
    luaL_unref(L, LUA_REGISTRYINDEX, ctx->funcRef);
    luaL_unref(L, LUA_REGISTRYINDEX, ctx->userRef);
}


