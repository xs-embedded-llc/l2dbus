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
 * @file           l2dbus_watch.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus watch object
 *******************************************************************************
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_watch.h"
#include "l2dbus_dispatcher.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"
#include "ev.h"
#include "lualib.h"

static void
l2dbus_watchMakeEvTable
    (
    lua_State*      L,
    cdbus_UInt32    events
    )
{
    lua_createtable(L, 0, 5);
    lua_pushstring(L, "evMask");
    lua_pushinteger(L, events);
    lua_rawset(L, -3);

    lua_pushstring(L, "READ");
    lua_pushboolean(L, (events & DBUS_WATCH_READABLE) != 0);
    lua_rawset(L, -3);

    lua_pushstring(L, "WRITE");
    lua_pushboolean(L, (events & DBUS_WATCH_WRITABLE) != 0);
    lua_rawset(L, -3);

    lua_pushstring(L, "ERROR");
    lua_pushboolean(L, (events & DBUS_WATCH_ERROR) != 0);
    lua_rawset(L, -3);

    lua_pushstring(L, "HANGUP");
    lua_pushboolean(L, (events & DBUS_WATCH_HANGUP) != 0);
    lua_rawset(L, -3);

    /* Returns an event table on the top of the stack */
}

static cdbus_UInt32
l2dbus_watchParseEvents
    (
    lua_State*  L,
    int         idx
    )
{
    cdbus_UInt32 events = 0U;
    int i;
    const char* evStr;

    idx = lua_absindex(L, idx);

    if ( LUA_TNUMBER == lua_type(L, idx) )
    {
        events = (cdbus_UInt32)lua_tointeger(L, idx);
    }
    else if ( LUA_TSTRING == lua_type(L, idx) )
    {
        evStr = lua_tostring(L, idx);

        for ( i = strlen(evStr); i > 0; --i )
        {
            switch ( tolower(evStr[i-1]) )
            {
                case 'r':
                    events |= DBUS_WATCH_READABLE;
                    break;

                case 'w':
                    events |= DBUS_WATCH_WRITABLE;
                    break;

                case 'e':
                    events |= DBUS_WATCH_ERROR;
                    break;

                case 'h':
                    events |= DBUS_WATCH_HANGUP;
                    break;

                default:
                    break;
            }
        }
    }
    else
    {
        luaL_error(L, "bad argument #%d - expected number or string", idx);
    }

    if ( 0 == (events &
        (DBUS_WATCH_READABLE | DBUS_WATCH_WRITABLE |
         DBUS_WATCH_ERROR | DBUS_WATCH_HANGUP)) )
    {
        luaL_error(L, "bad argument #%d - no events specified", idx);
    }

    return events;
}


static cdbus_Bool
l2dbus_watchHandler
    (
    cdbus_Watch*    w,
    cdbus_UInt32    rcvEvents,
    void*           user
    )
{
    lua_State* L = l2dbus_callbackGetThread();
    const char* errMsg = "";
    l2dbus_Watch* ud = l2dbus_callbackFetchUd(L, user);

    /* Nil or the Watch userdata is sitting at the top of the
     * stack at this point.
     */

    assert( NULL != w );
    assert( NULL != L );

    /* If the watch userdata has been GC'ed then ... */
    if ( NULL == ud )
    {
        L2DBUS_TRACE((L2DBUS_TRC_WARN,
            "Cannot call handler because the watch has been GC'ed"));
    }
    else
    {
        /* Push function and user value on the stack and execute the callback */
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.funcRef);
        lua_pushvalue(L, -2 /* Watch ud */);
        l2dbus_watchMakeEvTable(L, rcvEvents);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);

        if ( 0 != lua_pcall(L, 3 /* nArgs */, 0, 0) )
        {
            if ( lua_isstring(L, -1) )
            {
                errMsg = lua_tostring(L, -1);
            }
            L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Watch callback error: %s", errMsg));
        }
    }

    /* Clean up the thread stack */
    lua_settop(L, 0);

    /* The return value is unused by CDBUS */
    return CDBUS_TRUE;
}


int
l2dbus_newWatch
    (
    lua_State*  L
    )
{
    l2dbus_Watch* watchUd;
    l2dbus_Dispatcher* dispUd;
    cdbus_UInt32 events = 0U;
    int nArgs;
    cdbus_Descriptor fd;
    FILE* fp;
    int userIdx = L2DBUS_CALLBACK_NOREF_NEEDED;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: watch"));

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

    /* Parse the file/descriptor */
    if ( (LUA_TUSERDATA == lua_type(L, 2)) &&
        luaL_checkudata(L, 2, LUA_FILEHANDLE) )
    {
        fp = *(FILE**)luaL_checkudata(L, 2, LUA_FILEHANDLE);
        fd = fileno(fp);
    }
    else if (LUA_TNUMBER == lua_type(L, 2) )
    {
        fd = lua_tointeger(L, 2);
    }
    else
    {
        luaL_error(L, "bad argument #2 - expected file object or stream descriptor");
    }


    /* Parse the events that should be watched */
    events = l2dbus_watchParseEvents(L, 3);

    /* Check for a handler function */
    luaL_checktype(L, 4, LUA_TFUNCTION);

    /* See if an optional user value is provided */
    if ( nArgs >= 5 )
    {
        userIdx = 5;
    }

    watchUd = (l2dbus_Watch*)l2dbus_objectNew(L, sizeof(*watchUd),
                                             L2DBUS_WATCH_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Watch userdata=%p", watchUd));

    if ( NULL == watchUd )
    {
        luaL_error(L, "Failed to create watch userdata!");
    }
    else
    {
        /* Reset the userdata structure */
        l2dbus_callbackInit(&watchUd->cbCtx);
        watchUd->dispUdRef = LUA_NOREF;

        l2dbus_callbackRef(L, 4 /* func */, userIdx, &watchUd->cbCtx);
        watchUd->watch = cdbus_watchNew(dispUd->disp, fd, events,
                                    l2dbus_watchHandler, watchUd);

        if ( NULL == watchUd->watch )
        {
            /* Release any references we may still have */
            l2dbus_callbackUnref(L, &watchUd->cbCtx);
            luaL_error(L, "Failed to allocate Watch");
        }
        else
        {
            /* Add a reference to the Dispatcher userdata */
            lua_pushvalue(L, 1 /* dispUd */);
            watchUd->dispUdRef = luaL_ref(L, LUA_REGISTRYINDEX);

            /* Create a weak reference to the Watch user data */
            l2dbus_callbackAddWeakRef(L, -1);
        }
    }

    return 1;
}



static int
l2dbus_watchDispose
    (
    lua_State*  L
    )
{
    l2dbus_Watch* ud = (l2dbus_Watch*)luaL_checkudata(L, -1,
                                        L2DBUS_WATCH_MTBL_NAME);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: watch (userdata=%p)", ud));

    if ( ud->watch != NULL )
    {
        cdbus_watchEnable(ud->watch, CDBUS_FALSE);
        cdbus_watchUnref(ud->watch);
    }

    /* Drop the weak reference to the userdata */
    l2dbus_callbackRemoveWeakRef(L, -1);

    /* We no longer need to anchor the dispatcher */
    luaL_unref(L, LUA_REGISTRYINDEX, ud->dispUdRef);

    /* Unreference the function/data associated with a callback */
    l2dbus_callbackUnref(L, &ud->cbCtx);

    return 0;
}


static int
l2dbus_watchGetDescriptor
    (
    lua_State*  L
    )
{
    l2dbus_Watch* ud = (l2dbus_Watch*)luaL_checkudata(L, 1,
                                        L2DBUS_WATCH_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_pushinteger(L, cdbus_watchGetDescriptor(ud->watch));

    return 1;
}


static int
l2dbus_watchEvents
    (
    lua_State*  L
    )
{
    l2dbus_Watch* ud = (l2dbus_Watch*)luaL_checkudata(L, 1,
                                                    L2DBUS_WATCH_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    l2dbus_watchMakeEvTable(L, cdbus_watchGetFlags(ud->watch));

    return 1;
}

static int
l2dbus_watchSetEvents
    (
    lua_State*  L
    )
{
    cdbus_UInt32 events;
    cdbus_HResult rc;
    l2dbus_Watch* ud = (l2dbus_Watch*)luaL_checkudata(L, 1,
                                                    L2DBUS_WATCH_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    events = l2dbus_watchParseEvents(L, 2);

    rc = cdbus_watchSetFlags(ud->watch, events);

    if ( CDBUS_FAILED(rc) )
    {
        l2dbus_cdbusError(L, rc, "Cannot set the watch I/O events");
    }

    return 0;
}


static int
l2dbus_watchIsEnabled
    (
    lua_State*  L
    )
{
    l2dbus_Watch* ud = (l2dbus_Watch*)luaL_checkudata(L, 1,
                                                L2DBUS_WATCH_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_pushboolean(L, cdbus_watchIsEnabled(ud->watch));
    return 1;
}


static int
l2dbus_watchSetEnable
    (
    lua_State*  L
    )
{
    cdbus_HResult rc;
    int enable;

    l2dbus_Watch* ud = (l2dbus_Watch*)luaL_checkudata(L, 1,
                                                    L2DBUS_WATCH_MTBL_NAME);
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    enable = lua_toboolean(L, 2);
    rc = cdbus_watchEnable(ud->watch, enable);
    if ( CDBUS_FAILED(rc) )
    {
        l2dbus_cdbusError(L, rc, "Cannot enable/disable watch");
    }

    return 0;
}


static int
l2dbus_watchData
    (
    lua_State*  L
    )
{
    l2dbus_Watch* ud = (l2dbus_Watch*)luaL_checkudata(L, 1,
                                                L2DBUS_WATCH_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);
    return 1;
}


static int
l2dbus_watchSetData
    (
    lua_State*  L
    )
{
    l2dbus_Watch* ud = (l2dbus_Watch*)luaL_checkudata(L, 1,
                                                L2DBUS_WATCH_MTBL_NAME);
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


static int
l2dbus_watchClearPending
    (
    lua_State*  L
    )
{
    l2dbus_Watch* ud = (l2dbus_Watch*)luaL_checkudata(L, 1,
                                                L2DBUS_WATCH_MTBL_NAME);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    l2dbus_watchMakeEvTable(L, cdbus_watchClearPending(ud->watch));

    return 1;
}


static const luaL_Reg l2dbus_watchMetaTable[] = {
    {"isEnabled", l2dbus_watchIsEnabled},
    {"setEnable", l2dbus_watchSetEnable},
    {"getDescriptor", l2dbus_watchGetDescriptor},
    {"events", l2dbus_watchEvents},
    {"setEvents", l2dbus_watchSetEvents},
    {"data", l2dbus_watchData},
    {"setData", l2dbus_watchSetData},
    {"clearPending", l2dbus_watchClearPending},
    {"__gc", l2dbus_watchDispose},
    {NULL, NULL},
};


void
l2dbus_openWatch
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_WATCH_TYPE_ID,
            l2dbus_watchMetaTable));
    lua_newtable(L);
    lua_pushcfunction(L, l2dbus_newWatch);
    lua_setfield(L, -2, "new");

    lua_pushstring(L, "READ");
    lua_pushinteger(L, DBUS_WATCH_READABLE);
    lua_rawset(L, -3);

    lua_pushstring(L, "WRITE");
    lua_pushinteger(L, DBUS_WATCH_WRITABLE);
    lua_rawset(L, -3);

    lua_pushstring(L, "ERROR");
    lua_pushinteger(L, DBUS_WATCH_ERROR);
    lua_rawset(L, -3);

    lua_pushstring(L, "HANGUP");
    lua_pushinteger(L, DBUS_WATCH_HANGUP);
    lua_rawset(L, -3);
}



