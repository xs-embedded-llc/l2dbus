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
 * @file           l2dbus_dispatcher.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of Lua Dispatcher binding.
 *===========================================================================
 */
#include <stdlib.h>
#include "cdbus/cdbus.h"
#include "l2dbus_alloc.h"
#include "l2dbus_compat.h"
#include "l2dbus_dispatcher.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"
#include "l2dbus_callback.h"
#include "l2dbus_main-loop.h"

/**
 The L2DBUS Event Dispatcher Object

 This namespace provides a constructor and methods for manipulating the
 D-Bus event Dispatcher object. A Dispatcher is responsible for
 running the event loop that detects incoming D-Bus messages and
 timeouts and also dispatches outgoing messages and signals.
 @namespace l2dbus.Dispatcher
 */


/*
 * This function is called when the *last* reference to an underlying
 * CDBUS dispatcher is called. It's only called in the case when a "foreign"
 * libev loop is being utilized (e.g. we don't own it but it's owned by
 * another module like the Lua libev binding).
 */
static void
l2dbus_dispatcherFinalized
    (
    void*   data
    )
{
    int* loopRef = (int*)data;
    if ( NULL != loopRef )
    {
        L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Unreferencing the foreign main loop"));
        luaL_unref(l2dbus_callbackGetThread(), LUA_REGISTRYINDEX, *loopRef);
        l2dbus_free(loopRef);
    }
}


/**
 @function new

 Creates a new L2DBUS Dispatcher.

 Constructs a new Dispatcher using the provided main loop.

 @tparam userdata Main loop to be used by the dispatcher.
 @treturn userdata Dispatcher userdata object
 */
int
l2dbus_newDispatcher
    (
    lua_State*  L
    )
{
    l2dbus_Dispatcher* dispUd;
    l2dbus_MainLoopUserData* loopUd;
    int* loopRef = NULL;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: dispatcher"));

    /* Make sure the module wasn't shutdown */
    l2dbus_checkModuleInitialized(L);

    loopUd = (l2dbus_MainLoopUserData*)
                            luaL_checkudata(L, 1, L2DBUS_MAIN_LOOP_MTBL_NAME);


    dispUd = (l2dbus_Dispatcher*)l2dbus_objectNew(L, sizeof(*dispUd),
                                    L2DBUS_DISPATCHER_TYPE_ID);
    dispUd->finalizerRef = LUA_NOREF;
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Dispatcher userdata=%p", dispUd));
    if ( NULL == dispUd )
    {
        luaL_error(L, "Failed to allocate Dispatcher userdata!");
    }

    dispUd->disp = cdbus_dispatcherNew(loopUd->loop);
    if ( NULL == dispUd->disp )
    {
        luaL_error(L, "Failed to allocate Dispatcher!");
    }

    /* If we don't own the loop then we need to at least reference it */

    loopRef = (int*)l2dbus_malloc(sizeof(*loopRef));
    if ( NULL == loopRef )
    {
        cdbus_dispatcherUnref(dispUd->disp);
        dispUd->disp = NULL;
        luaL_error(L,
            "Failed to allocate memory for libev loop reference!");
    }
    else
    {
        lua_pushvalue(L, 1);
        *loopRef = luaL_ref(L, LUA_REGISTRYINDEX);

        /* This function is called when the *last* reference to the
         * CDBUS dispatcher is dropped. At that point we'll break the
         * strong reference to the loop. If we break the
         * reference any sooner then there is a chance that as D-Bus is
         * shut-down an associated Watch may try to access the
         * loop. If it's already been GC'ed by Lua then it may
         * not be around and we'll see an assertion.
         */
        cdbus_dispatcherSetFinalizer(dispUd->disp,
                                    l2dbus_dispatcherFinalized,
                                    loopRef);
    }

    dispUd->finalizerRef = l2dbus_moduleFinalizerRef(L);

    return 1;
}


/**
 * A Dispatcher class.
 * @type Dispatcher
 */


/**
 @function run
 @within Dispatcher

 Invokes the dispatcher to process messages and timeouts.

 This method should be called to process one or more events
 depending on the *run* option provided.

 @tparam userdata disp The Dispatcher instance.
 @tparam number runOpt One of the constant run options
 @treturn bool Returns **true** if dispatcher successfully ran
 one or more event iterations. Returns **false** on error.
 @treturn ?string|nil An error message in the event of an error
 */
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
        lua_pushboolean(L, L2DBUS_FALSE);
        lua_pushfstring(L, "Failed to run dispatcher (errCode=%f)", (lua_Number)rc);
        return 2;
    }

    lua_pushboolean(L, L2DBUS_TRUE);

    return 1;
}


/**
 @function stop
 @within Dispatcher

 Stops the dispatcher and causes @{run} to exit.

 This is used to break out of the dispatch loop entered by calling
 @{run} on the dispatcher. A Lua error is thrown if the dispatcher
 cannot be stopped.

 @tparam userdata disp The Dispatcher instance.
 @treturn bool True if dispatcher stops
 */
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


/**
 * @brief Called by Lua VM to GC/reclaim the Dispatcher userdata.
 *
 * This method is called by the Lua VM to reclaim the Dispatcher
 * userdata.
 *
 * @return nil
 *
 */
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
        cdbus_dispatcherUnref(ud->disp);
        ud->disp = NULL;
        l2dbus_moduleFinalizerUnref(L, ud->finalizerRef);

        /* Any "foreign" libev loop that we don't own will be unreferenced
         * later when the last reference to the underlying CDBUS dispatcher
         * is dropped.
         */
    }

    return 0;
}


/*
 * Define the methods of the Dispatcher
 */
static const luaL_Reg l2dbus_dispatcherMetaTable[] = {
    {"run", l2dbus_dispatcherRun},
    {"stop", l2dbus_dispatcherStop},
    {"__gc", l2dbus_dispatcherDispose},
    {NULL, NULL},
};


/**
 @brief Creates the Dispatcher sub-module.

 This function creates a metatable entry for the Dispatcher userdata
 and simulates opening the Dispatcher sub-module.

 @return nil
 */
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

/**
 @constant DISPATCH_WAIT
 Calls to @{run} with this option block waiting for successive events until @{stop} is called on
 the Dispatcher.
 */
    lua_pushinteger(L, CDBUS_RUN_WAIT);
    lua_setfield(L, -2, "DISPATCH_WAIT");

/**
 @constant DISPATCH_NO_WAIT
 Calls to @{run} with this option do <b>not</b> block waiting for events and it immediately
 returns if there are no events to dispatch. If an event is already ready to be dispatched it
 will be processed.
 */
    lua_pushinteger(L, CDBUS_RUN_NO_WAIT);
    lua_setfield(L, -2, "DISPATCH_NO_WAIT");

/**
 @constant DISPATCH_ONCE
 Calls to @{run} with this option block waiting for one event before immediately returning.
 */
    lua_pushinteger(L, CDBUS_RUN_ONCE);
    lua_setfield(L, -2, "DISPATCH_ONCE");
}




