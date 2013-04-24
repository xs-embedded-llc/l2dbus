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
 * @file           l2dbus_watch.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus watch object
 *===========================================================================
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

/**
 L2DBUS Watch

 This section describes a L2DBUS Watch class which represents a file
 descriptor that can be monitored for activity. On Linux platforms this
 can include sockets, pipes, files, and anything that yields a poll-able
 descriptor. Internally, L2DBUS monitors the file-descriptor associated
 with a @{l2dbus.Connection|Connection} to detect when new messages are
 available to be received. Client applications can use this same mechanism
 to monitor activity on various descriptors themselves.

 @namespace l2dbus.Watch
 */

/**
 The EventTable provides an indication of what events are signaled on
 a given file-descriptor. The *evMask* field is a bitwise **OR** of a
 combination of the following constants: @{READ}, @{WRITE}, @{ERROR}, and
 @{HANGUP}. There are also discrete fields in the table for each event type
 and these will be set to **true** or **false** depending on whether they are
 signaled.

 @table EventTable
 @field evMask (number) The actual event mask of signaled events.
 @field READ (bool) Set to **true** if the *read* event is signaled.
 @field WRITE (bool) Set to **true** if the *write* event is signaled.
 @field ERROR (bool) Set to **true** if the *error* event is signaled.
 @field HANGUP (bool) Set to **true** if the *hangup* event is signaled.
 */


/**
 * @brief Creates a Lua table containing file descriptor event information.
 *
 * This function will leave on the Lua stack a table that represents the
 * events that are passed into the function.
 *
 * @param [in] L      Lua state.
 * @param [in] event  A bit-mask of signaled file-descriptor events.
 * @return A table on the Lua stack containing the event mask itself as
 * well as the actual events broken out.
 */
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


/**
 * @brief A helper function to parse specified events from Lua
 *
 * This function will interrogate a Lua parameter on the stack and
 * attempt to deduce which file descriptor events are specified by the
 * parameter. The parameter can be a number in which case it's assumed to
 * be a bitmask of event types. If it's a string then it's assumed to
 * contain the characters 'r' (READ), 'w' (WRITE), 'e' (ERROR), and
 * 'h' (HANGUP).
 *
 * @param [in] L    Lua state.
 * @param [in] idx  The index on the Lua stack for the event parameter.
 * Expected to be a Lua number or string.
 */
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


/**
 * @brief Processes watch events from the underlying CDBUS callback.
 *
 * This function translates and dispatches CDBUS Watch events to the associated
 * Lua Watch handler.
 *
 * @param [in] w            CDBUS Watch instance.
 * @param [in] rcvEvents    The bitmask of signaled events.
 * @param [in] user         Client/user data associated with the Watch.
 * @return A boolean value that is ignored by CDBUS.
 */
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
    l2dbus_Watch* ud = l2dbus_objectRegistryGet(L, user);

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


/**
 @function new

 Creates a new Watch.

 Creates a new Watch with an associated handler that is called whenever one
 of the specified events is signaled on the provided file descriptor.
 The Watch handler has a signature of the form:

    function onWatch(watch, evTable, userToken)

 Where:

 <ul>
 <li>*watch*        - The L2DBUS Watch instance</li>
 <li>*evTable*      - An table of signaled events. See @{EventTable}.</li>
 <li>*userToken*    - A value specified by the client when the watch is created.</li>
 </ul>

 The handler does not have to return anything but should exit quickly to
 minimize interruptions to the @{l2dbus.Dispatcher|Dispatcher} main loop.

 @tparam userdata The @{l2dbus.Dispatcher|dispatcher} with which to associate
 the Watch.
 @tparam ?number|userdata fd The file descriptor to watch. This can be either
 a Lua file (e.g. userdata wrapping the actual file descriptor) or the
 raw integral file descriptor received from the operating system that is
 pollable.
 @tparam number|string events The events (@{READ}, @{WRITE}, @{ERROR},
 @{HANGUP}) which should be monitored. If the parameter is a Lua number then
 it is assumed to be a bitmask of the previous constants **OR**'ed together. If
 it is a string then the letters *r* (@{READ}), *w* (@{WRITE}), *e* (@{ERROR}),
 and *h* (@{HANGUP}) are expected to indicate the events to watch.
 @tparam func handler The watch handler that's called when a timeout expires.
 @tparam ?any userToken User data that will be passed to the Watch
 handler when it's called. Can be any Lua value.
 @treturn userdata The userdata object representing the Watch.
 */
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
    cdbus_Descriptor fd = -1;
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
            l2dbus_objectRegistryAdd(L, watchUd, -1);
        }
    }

    return 1;
}



/**
 * @brief Called by Lua VM to GC/reclaim the Watch userdata.
 *
 * This method is called by the Lua VM to reclaim the Watch userdata.
 *
 * @return nil
 *
 */
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
    l2dbus_objectRegistryRemove(L, ud);

    /* We no longer need to anchor the dispatcher */
    luaL_unref(L, LUA_REGISTRYINDEX, ud->dispUdRef);

    /* Unreference the function/data associated with a callback */
    l2dbus_callbackUnref(L, &ud->cbCtx);

    return 0;
}


/**
 * The L2DBUS Watch class.
 * @type Watch
 */

/**
 @function getDescriptor
 @within Watch

 Returns the underlying file descriptor being watched.

 @tparam userdata watch The watch monitoring the file descriptor.
 @treturn number Returns the underlying OS file descriptor being monitored.
 */
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


/**
 @function events
 @within Watch

 Returns the events being monitored on the file descriptor.

 @tparam userdata watch The watch for which the events are returned.
 @treturn table Returns an @{EventTable} indicating the events that
 are being monitored.
 */
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


/**
 @function setEvents
 @within Watch

 Sets the events that the Watch should monitor.

 @tparam userdata watch The watch to set the events to monitor.
 @tparam number|string events The events (@{READ}, @{WRITE}, @{ERROR},
 @{HANGUP}) which should be monitored. If the parameter is a Lua number then
 it is assumed to be a bitmask of the previous constants **OR**'ed together. If
 it is a string then the letters *r* (@{READ}), *w* (@{WRITE}), *e* (@{ERROR}),
 and *h* (@{HANGUP}) are expected to indicate the events to watch.
 */
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


/**
 @function isEnabled
 @within Watch

 Returns whether the specified watch is enabled to monitor events.

 @tparam userdata watch The watch to see if it's enabled.
 @treturn bool Returns **true** if it's enabled, **false** otherwise.
 */
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


/**
 @function setEnable
 @within Watch

 Sets whether the watch should be enabled or disabled from monitoring the
 associated file descriptor.

 @tparam userdata watch The watch to configure.
 @tparam bool option Set to **true** to enable the watch, **false** to
 disable it.
 */
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


/**
 @function data
 @within Watch

 Returns the user specified data associated with the watch.

 @tparam userdata watch The watch to get the user data.
 @treturn any Returns the user data associated with the watch.
 */
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


/**
 @function setData
 @within Watch

 Sets the user specific data passed to the watch handler.

 @tparam userdata watch The watch to set the user data.
 @tparam any userToken The user specific data to associate with the watch.
 */
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


/**
 @function clearPending
 @within Watch

 Retrieves any signaled/pending events for a descriptor and then clears them.

 If the watch has pending events it will return these pending events. If an
 event was signaled/pending the associated flag is cleared (and therefore this
 watch event will not trigger the handler function).

 @tparam userdata watch The watch to clear the events.
 @tparam table events Returns an @{EventTable} of signaled/pending events.
 */
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


/*
 * Define the methods of the Watch class
 */
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


/**
 * @brief Creates the Watch sub-module.
 *
 * This function creates a metatable entry for the Watch userdata
 * and simulates opening the Watch sub-module.
 *
 * @return A table defining the Watch sub-module.
 */
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

/**
 @constant READ
 The bitmask for file-descriptor *read* event.
 */
    lua_pushstring(L, "READ");
    lua_pushinteger(L, DBUS_WATCH_READABLE);
    lua_rawset(L, -3);

/**
 @constant WRITE
 The bitmask for file-descriptor *write* event.
 */
    lua_pushstring(L, "WRITE");
    lua_pushinteger(L, DBUS_WATCH_WRITABLE);
    lua_rawset(L, -3);

/**
 @constant ERROR
 The bitmask for file-descriptor *error* event.
 */
    lua_pushstring(L, "ERROR");
    lua_pushinteger(L, DBUS_WATCH_ERROR);
    lua_rawset(L, -3);

/**
 @constant HANGUP
 The bitmask for file-descriptor *hangup* event.
 */
    lua_pushstring(L, "HANGUP");
    lua_pushinteger(L, DBUS_WATCH_HANGUP);
    lua_rawset(L, -3);
}



