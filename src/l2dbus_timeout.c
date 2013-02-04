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
 * @file           l2dbus_timeout.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of a D-Bus timeout object
 *******************************************************************************
 */
#include <stdlib.h>
#include <assert.h>
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_timeout.h"
#include "l2dbus_dispatcher.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"


static cdbus_Bool
l2dbus_timeoutHandler
    (
    cdbus_Timeout*  t,
    void*           user
    )
{
    lua_State* L = l2dbus_callbackGetThread();
    const char* errMsg = "";
    l2dbus_Timeout* ud = l2dbus_callbackFetchUd(L, user);

    /* Nil or the Timeout userdata is sitting at the top of the
     * stack at this point.
     */

    assert( NULL != t );
    assert( NULL != L );

    /* If the timeout userdata has been GC'ed then ... */
    if ( NULL == ud )
    {
        L2DBUS_TRACE((L2DBUS_TRC_WARN,
            "Cannot call handler because the timeout has been GC'ed"));
    }
    else
    {
        /* Push function and user value on the stack and execute the callback */
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.funcRef);
        lua_pushvalue(L, -2 /* Timeout ud */);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);

        if ( 0 != lua_pcall(L, 2 /* nArgs */, 0, 0) )
        {
            if ( lua_isstring(L, -1) )
            {
                errMsg = lua_tostring(L, -1);
            }
            L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Timeout callback error: %s", errMsg));
        }
    }

    /* Clean up the thread stack */
    lua_settop(L, 0);

    /* The return value is unused by CDBUS */
    return CDBUS_TRUE;
}


int
l2dbus_newTimeout
    (
    lua_State*  L
    )
{
    l2dbus_Timeout* timeoutUd;
    l2dbus_Dispatcher* dispUd;
    int msecInterval;
    l2dbus_Bool repeat;
    int nArgs;
    int userIdx = L2DBUS_CALLBACK_NOREF_NEEDED;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: timeout"));

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    /* See how many arguments were passed in */
    nArgs = lua_gettop(L);

    if ( nArgs <  4 )
    {
        luaL_error(L, "Insufficient number of parameters");
    }


    dispUd = (l2dbus_Dispatcher*)luaL_checkudata(L, 1,
                                    L2DBUS_DISPATCHER_MTBL_NAME);
    msecInterval = luaL_checkint(L, 2);
    luaL_checktype(L, 3, LUA_TBOOLEAN);
    repeat = lua_toboolean(L, 3);
    luaL_checktype(L, 4, LUA_TFUNCTION);

    /* See if an optional user value is provided */
    if ( nArgs >= 5 )
    {
        userIdx = 5;
    }

    timeoutUd = (l2dbus_Timeout*)l2dbus_objectNew(L, sizeof(*timeoutUd),
                                                    L2DBUS_TIMEOUT_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Timeout userdata=%p", timeoutUd));

    if ( NULL == timeoutUd )
    {
        luaL_error(L, "Failed to create timeout userdata!");
    }
    else
    {
        /* Reset the userdata structure */
        l2dbus_callbackInit(&timeoutUd->cbCtx);
        timeoutUd->dispUdRef = LUA_NOREF;

        l2dbus_callbackRef(L, 4 /* func */, userIdx, &timeoutUd->cbCtx);
        timeoutUd->timeout = cdbus_timeoutNew(dispUd->disp, msecInterval, repeat,
                                    l2dbus_timeoutHandler, timeoutUd);

        if ( NULL == timeoutUd->timeout )
        {
            /* Release any references we may still have */
            l2dbus_callbackUnref(L, &timeoutUd->cbCtx);
            luaL_error(L, "Failed to allocate Timeout");
        }
        else
        {
            /* Add a reference to the Dispatcher userdata */
            lua_pushvalue(L, 1 /* dispUd */);
            timeoutUd->dispUdRef = luaL_ref(L, LUA_REGISTRYINDEX);

            /* Create a weak reference to the Timeout user data */
            l2dbus_objectRegistryAdd(L, timeoutUd, -1);
        }
    }

    return 1;
}



static int
l2dbus_timeoutDispose
    (
    lua_State*  L
    )
{
    l2dbus_Timeout* ud = (l2dbus_Timeout*)luaL_checkudata(L, -1,
                                        L2DBUS_TIMEOUT_MTBL_NAME);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: timeout (userdata=%p)", ud));

    if ( ud->timeout != NULL )
    {
        cdbus_timeoutEnable(ud->timeout, CDBUS_FALSE);
        cdbus_timeoutUnref(ud->timeout);
    }

    /* Drop the weak reference to the userdata */
    l2dbus_objectRegistryRemove(L, ud);

    /* We no longer need to anchor the dispatcher */
    luaL_unref(L, LUA_REGISTRYINDEX, ud->dispUdRef);

    /* Unreference and function/data associated with a callback */
    l2dbus_callbackUnref(L, &ud->cbCtx);

    return 0;
}


static int
l2dbus_timeoutIsEnabled
    (
    lua_State*  L
    )
{
    l2dbus_Timeout* ud = (l2dbus_Timeout*)luaL_checkudata(L, 1,
                                                L2DBUS_TIMEOUT_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_pushboolean(L, cdbus_timeoutIsEnabled(ud->timeout));
    return 1;
}


static int
l2dbus_timeoutSetEnable
    (
    lua_State*  L
    )
{
    cdbus_HResult rc;
    int enable;

    l2dbus_Timeout* ud = (l2dbus_Timeout*)luaL_checkudata(L, 1,
                                                    L2DBUS_TIMEOUT_MTBL_NAME);
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    enable = lua_toboolean(L, 2);
    rc = cdbus_timeoutEnable(ud->timeout, enable);
    if ( CDBUS_FAILED(rc) )
    {
        l2dbus_cdbusError(L, rc, "Cannot enable/disable timer");
    }

    return 0;
}


static int
l2dbus_timeoutInterval
    (
    lua_State*  L
    )
{
    l2dbus_Timeout* ud = (l2dbus_Timeout*)luaL_checkudata(L, 1,
                                                L2DBUS_TIMEOUT_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_pushinteger(L, cdbus_timeoutInterval(ud->timeout));
    return 1;
}

static int
l2dbus_timeoutSetInterval
    (
    lua_State*  L
    )
{
    cdbus_HResult rc;
    cdbus_Int32 interval;

    l2dbus_Timeout* ud = (l2dbus_Timeout*)luaL_checkudata(L, 1,
                                                    L2DBUS_TIMEOUT_MTBL_NAME);
    interval = luaL_checkint(L, 2);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    rc = cdbus_timeoutSetInterval(ud->timeout, interval);
    if ( CDBUS_FAILED(rc) )
    {
        l2dbus_cdbusError(L, rc, "Cannot set the timeout interval");
    }

    return 0;
}


static int
l2dbus_timeoutRepeat
    (
    lua_State*  L
    )
{
    l2dbus_Timeout* ud = (l2dbus_Timeout*)luaL_checkudata(L, 1,
                                                L2DBUS_TIMEOUT_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_pushboolean(L, cdbus_timeoutGetRepeat(ud->timeout));
    return 1;
}


static int
l2dbus_timeoutSetRepeat
    (
    lua_State*  L
    )
{
    l2dbus_Timeout* ud = (l2dbus_Timeout*)luaL_checkudata(L, 1,
                                                    L2DBUS_TIMEOUT_MTBL_NAME);
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    cdbus_timeoutSetRepeat(ud->timeout,  lua_toboolean(L, 2));

    return 0;
}


static int
l2dbus_timeoutData
    (
    lua_State*  L
    )
{
    l2dbus_Timeout* ud = (l2dbus_Timeout*)luaL_checkudata(L, 1,
                                                L2DBUS_TIMEOUT_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);
    return 1;
}


static int
l2dbus_timeoutSetData
    (
    lua_State*  L
    )
{
    l2dbus_Timeout* ud = (l2dbus_Timeout*)luaL_checkudata(L, 1,
                                                L2DBUS_TIMEOUT_MTBL_NAME);
    /* Any value is acceptable - but it should be specified */
    luaL_checkany(L, 2);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    /* Unreference the previous value */
    luaL_unref(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);

    /* On the top of the stack should be the client's user data value.
     * We'll keep a reference to that for safe-keeping.
     */
    ud->cbCtx.userRef = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}


static const luaL_Reg l2dbus_timeoutMetaTable[] = {
    {"isEnabled", l2dbus_timeoutIsEnabled},
    {"setEnable", l2dbus_timeoutSetEnable},
    {"interval", l2dbus_timeoutInterval},
    {"setInterval", l2dbus_timeoutSetInterval},
    {"repeats", l2dbus_timeoutRepeat},
    {"setRepeat", l2dbus_timeoutSetRepeat},
    {"data", l2dbus_timeoutData},
    {"setData", l2dbus_timeoutSetData},
    {"__gc", l2dbus_timeoutDispose},
    {NULL, NULL},
};


void
l2dbus_openTimeout
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_TIMEOUT_TYPE_ID,
            l2dbus_timeoutMetaTable));
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, l2dbus_newTimeout);
    lua_setfield(L, -2, "new");
}


