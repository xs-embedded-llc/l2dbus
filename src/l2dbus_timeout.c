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
 * @file           l2dbus_timeout.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of a D-Bus timeout object
 *===========================================================================
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

/**
 L2DBUS Timeout

 This section describes a L2DBUS Timeout class.

 The L2DBUS Timeout class exposes methods that provide for the creation of
 timeouts, enabling/disabling timeouts, and setting the repetition rate.
 Although normally used internally by L2DBUS, timeouts are useful for creating
 functions that will be called periodically in the future. Timeouts are driven
 (and dispatched) from the @{l2dbus.Dispatcher|Dispatcher} main loop so the
 @{l2dbus.Dispatcher.run|run} method must be called frequently in order to
 trigger timeout callbacks. As a result, the timeouts may not be a highly
 accurate timing mechanism depending on how much time is spent in these timer
 callback routines. The timeout callback should **not** block the thread of
 execution or the L2DBUS dispatch loop will block as well. Every effort should
 be made to exit the timeout handler quickly.


 While a Timeout is enabled a *strong* reference will be kept to it regardless
 of whether is referenced by the client code. This means that it is **not**
 eligible for garbage collection (GC) by the Lua VM. The strong reference
 will be maintained until the first timeout occurs. At the point, if the timeout
 remains enabled and is **not** configured to *repeat* then the strong
 reference will be dropped. If the timeout is enabled and repeatable then
 the strong reference is always maintained. This behavior facilitates the
 creation of non-referenced timeouts that will continue to trigger as long
 as they remain enabled and configured to repeat. It also supports the
 concept of a *one-shot* timeout that will automatically be reclaimed by the
 GC after firing once (when it's not set to repeat) when no external references
 are maintained.

 @namespace l2dbus.Timeout
 */


/**
 * @brief Handles and processes timeout callbacks.
 *
 * This function will try to deliver a callback to a Lua timeout handler
 * when invoked.
 *
 * @param [in] t      The CDBUS timeout instance.
 * @param [in] user   Opaque data provided by the client when the handler
 * was registered.
 * @return A boolean value that is currently unused by CDBUS.
 */
static cdbus_Bool
l2dbus_timeoutHandler
    (
    cdbus_Timeout*  t,
    void*           user
    )
{
    lua_State* L = l2dbus_callbackGetThread();
    const char* errMsg = "";
    l2dbus_Timeout* ud = l2dbus_objectRegistryGet(L, user);

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
            L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Timeout callback error: %s",
                          errMsg));
        }

        /*
         * If we're in the scenario where the timeout is enabled but it's not
         * configured to repeat then we need to un-reference it or it could
         * never be garbage collected.
         */
        if ( cdbus_timeoutIsEnabled(ud->timeout) &&
            !cdbus_timeoutGetRepeat(ud->timeout) )
        {
            /* Remove the reference since the timeout is now eligible to
             * be garbage collected.
             */
            luaL_unref(L, LUA_REGISTRYINDEX, ud->timeoutUdRef);
            ud->timeoutUdRef = LUA_NOREF;
        }
    }

    /* Clean up the thread stack */
    lua_settop(L, 0);

    /* The return value is unused by CDBUS */
    return CDBUS_TRUE;
}


/**
 @function new

 Creates a new Timeout.

 Creates a new Timeout with an associated handler that can be called (possibly
 repeatedly) as the timeout expires. A newly created Timeout is **disabled**
 by default and must explicitly be enabled in order to arm it. A handler
 (callback) is associated with a timeout and it is called when the
 timeout expires. The signature of the handler has the form:

    function onTimeout(timeout, userToken)

 Where:

 <ul>
 <li>*timeout*    - The L2DBUS Timeout instance</li>
 <li>*userToken*  - A value specified by the user when the timeout was created.</li>
 </ul>

 The handler does not have to return anything but should exit quickly to
 minimize interruption to the @{l2dbus.Dispatcher|Dispatcher} main loop.

 @tparam userdata dispatcher The @{l2dbus.Dispatcher|dispatcher} with which to associate
 the Timeout.
 @tparam number interval The amount of time (in millisecond) before the timeout
 will expire.
 @tparam bool repeat Set to **true** if the the timeout should reset after
 expiring (and starta agin) or **false** if it should only fire once.
 @tparam func handler The timeout handler that's called when a timeout expires.
 @tparam ?any userToken User data that will be passed to the Timeout
 handler when it's called. Can be any Lua value.
 @treturn userdata The userdata object representing the Timeout.
 */
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
        timeoutUd->timeoutUdRef = LUA_NOREF;

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


/**
 * @brief Called by Lua VM to GC/reclaim the Timeout userdata.
 *
 * This method is called by the Lua VM to reclaim the Timeout userdata.
 *
 * @return nil
 *
 */
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


/**
 * The L2DBUS Timeout class.
 * @type Timeout
 */

/**
 @function isEnabled
 @within Timeout

 Tests to determine whether the timeout is enabled.

 @tparam userdata timeout The timeout to test.
 @treturn bool Returns **true** if the timeout is enabled and **false**
 otherwise.
 */
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


/**
 @function setEnable
 @within Timeout

 Enables or disables a timeout.

 Internally a strong reference to an enabled timeout will be maintained
 until it triggers and then it will be further maintained if set to
 repeat. This can prevent the Lua GC from reclaiming a timeout that is
 enabled and set to *repeat*. By default a newly created timeout is *disabled*
 and must explicitly be enabled in order to activate it.

 @tparam userdata timeout The timeout to set.
 @tparam bool option Set to **true** to enable the timeout or **false** to
 disable it.
 */
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

    /*
     * While a timer is enabled a strong reference is kept to it so the
     * GC won't reclaim it. This addresses the situation where you have
     * an "anonymous" timeout that is enabled and needs to fire but no
     * one has kept a reference to it. After firing the strong reference is
     * maintained only if the timeout remains enabled and it's set to repeat.
     */
    if ( enable )
    {
        lua_pushvalue(L, 1 /* timeoutUd */);
        ud->timeoutUdRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        /* We no longer need to anchor the timer when it's disabled */
        luaL_unref(L, LUA_REGISTRYINDEX, ud->timeoutUdRef);
        ud->timeoutUdRef = LUA_NOREF;
    }

    return 0;
}


/**
 @function interval
 @within Timeout

 Returns the timeout interval (in milliseconds).

 @tparam userdata timeout The timeout to get the interval.
 @treturn number Returns the timeout interval in milliseconds.
 */
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


/**
 @function setInterval
 @within Timeout

 Sets the timeout interval.

 The timeout interval must be a positive number and specifies the number
 of milliseconds before the timeout expires.

 @tparam userdata timeout The timeout to set the interval.
 @tparam number interval The interval of the timeout (in milliseconds).
 */
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


/**
 @function repeats
 @within Timeout

 Returns whether or not the timeout is configured to repeatedly fire.

 @tparam userdata timeout The timeout to determine if it repeats.
 @treturn bool Returns **true** if the timeout is configured to repeat and
 **false** otherwise.
 */
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


/**
 @function setRepeat
 @within Timeout

 Configures the timeout to repeat or not.

 If the timeout is set to repeat then it will reset after firing and begin
 counting down to the next timeout period.

 @tparam userdata timeout The timeout to set the repeat option.
 @tparam bool option Set to **true** to enable the timeout to repeatedly fire
 or **false** so that it fires only once.
 */
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


/**
 @function data
 @within Timeout

 Returns the user specified data associated with the timeout.

 @tparam userdata timeout The timeout to get the user data.
 @treturn any Returns the user data associated with the timeout.
 */
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



/**
 @function setData
 @within Timeout

 Sets the user specific data passed to the timeout handler.

 @tparam userdata timeout The timeout to set the user data.
 @tparam any userToken The user specific data to associate with the timeout.
 */
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


/*
 * Define the methods of the Timeout class
 */
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


/**
 * @brief Creates the Timeout sub-module.
 *
 * This function creates a metatable entry for the Timeout userdata
 * and simulates opening the Timeout sub-module.
 *
 * @return A table defining the Timeout sub-module.
 */
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


