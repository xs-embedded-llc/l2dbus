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
 * @file           l2dbus_callback.c        
 * @author         Glenn Schmottlach
 * @brief          Callback implementation details
 *===========================================================================
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


