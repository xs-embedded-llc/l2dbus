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
 * @file           l2dbus_dispatcher.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of Lua Dispatcher binding.
 *******************************************************************************
 */
#include <stdlib.h>
#include "ev.h"
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_dispatcher.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"

#define L2DBUS_LIBEV_UNINITIALIZED_DEFAULT_LOOP ((struct ev_loop*)1)

int
l2dbus_newDispatcher
    (
    lua_State*  L
    )
{
    l2dbus_Dispatcher* userData;
    struct ev_loop* loop = NULL;
    int ownsLoop = 0;
    int loopIdx= 0;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: dispatcher"));

    /* Make sure the module wasn't shutdown */
    l2dbus_checkModuleInitialized(L);

    /* Check to see if a Lua libev loop userdata was passed
     * in for use as the main loop.
     */
    if ( LUA_TUSERDATA == lua_type(L, -1) )
    {
        loop = l2dbus_isEvLoop(L, -1);
        if ( loop == L2DBUS_LIBEV_UNINITIALIZED_DEFAULT_LOOP )
        {
            luaL_error(L, "The Lua libev loop is uninitialized - "
                    "try using ev.Loop.new() to create one");
        }
        ownsLoop = 0;
        loopIdx = lua_absindex(L, -1);
    }
    else
    {
        loop = ev_loop_new(EVFLAG_AUTO);
        ownsLoop = 1;
    }


    if ( NULL == loop )
    {
        luaL_error(L, "Failed to assign libev loop!");
    }
    else
    {
        userData = (l2dbus_Dispatcher*)l2dbus_objectNew(L, sizeof(*userData),
                                        L2DBUS_DISPATCHER_TYPE_ID);
        userData->loopRef = LUA_NOREF;
        userData->finalizerRef = LUA_NOREF;
        L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Dispatcher userdata=%p", userData));
        if ( NULL == userData )
        {
            if ( ownsLoop )
            {
                ev_loop_destroy(loop);
            }
            luaL_error(L, "Failed to allocate Dispatcher userdata!");
        }

        userData->disp = cdbus_dispatcherNew(loop, ownsLoop, NULL, NULL);
        if ( NULL == userData->disp )
        {
            if ( ownsLoop )
            {
                ev_loop_destroy(loop);
            }
            luaL_error(L, "Failed to allocate Dispatcher!");
        }

        /* If we don't own the loop then we need to at least reference it */
        if ( !ownsLoop )
        {
            lua_pushvalue(L, loopIdx);
            userData->loopRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }

        userData->finalizerRef = l2dbus_moduleFinalizerRef(L);
    }

    return 1;
}


static int
l2dbus_dispatcherRun
    (
    lua_State*  L
    )
{
    cdbus_HResult rc;
    int runOpt = CDBUS_RUN_NO_WAIT;
    l2dbus_Dispatcher* ud = (l2dbus_Dispatcher*)luaL_checkudata(L,
                                    1, L2DBUS_DISPATCHER_MTBL_NAME);

    /* Make sure the module wasn't shutdown */
    l2dbus_checkModuleInitialized(L);

    if ( lua_gettop(L) > 1 )
    {
        runOpt = luaL_checkint(L, 2);
    }

    switch ( runOpt )
    {
        case CDBUS_RUN_WAIT:
        case CDBUS_RUN_NO_WAIT:
        case CDBUS_RUN_ONCE:
            break;
        default:
            luaL_error(L, "Unknown run option (%d)", runOpt);
            break;
    }

    rc = cdbus_dispatcherRun(ud->disp, (cdbus_RunOption)runOpt);
    if ( CDBUS_FAILED(rc) )
    {
        l2dbus_cdbusError(L, rc, "Failed to run dispatcher");
    }

    lua_pushboolean(L, L2DBUS_TRUE);

    return 1;
}

static int
l2dbus_dispatcherStop
    (
    lua_State*  L
    )
{
    cdbus_HResult rc;

    l2dbus_Dispatcher* ud = (l2dbus_Dispatcher*)luaL_checkudata(L, 1, L2DBUS_DISPATCHER_MTBL_NAME);
    /* Make sure the module wasn't shutdown */
    l2dbus_checkModuleInitialized(L);

    rc = cdbus_dispatcherStop(ud->disp);
    if ( CDBUS_FAILED(rc) )
    {
        l2dbus_cdbusError(L, rc, "Failed to stop dispatcher");
    }

    lua_pushboolean(L, L2DBUS_TRUE);

    return 1;
}


static int
l2dbus_dispatcherDispose
    (
    lua_State*  L
    )
{
    l2dbus_Dispatcher* ud = (l2dbus_Dispatcher*)luaL_checkudata(L, -1, L2DBUS_DISPATCHER_MTBL_NAME);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: dispatcher (userdata=%p)", ud));

    if ( ud->disp != NULL )
    {
        luaL_unref(L, LUA_REGISTRYINDEX, ud->loopRef);
        cdbus_dispatcherUnref(ud->disp);
        ud->disp = NULL;
        l2dbus_moduleFinalizerUnref(L, ud->finalizerRef);
    }

    return 0;
}

static const luaL_Reg l2dbus_dispatcherMetaTable[] = {
    {"run", l2dbus_dispatcherRun},
    {"stop", l2dbus_dispatcherStop},
    {"__gc", l2dbus_dispatcherDispose},
    {NULL, NULL},
};


void
l2dbus_openDispatcher
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_DISPATCHER_TYPE_ID,
            l2dbus_dispatcherMetaTable));
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, l2dbus_newDispatcher);
    lua_setfield(L, -2, "new");

    /* Set Dispatcher constants */
    lua_pushinteger(L, CDBUS_RUN_WAIT);
    lua_setfield(L, -2, "DISPATCH_WAIT");
    lua_pushinteger(L, CDBUS_RUN_NO_WAIT);
    lua_setfield(L, -2, "DISPATCH_NO_WAIT");
    lua_pushinteger(L, CDBUS_RUN_ONCE);
    lua_setfield(L, -2, "DISPATCH_ONCE");
}




