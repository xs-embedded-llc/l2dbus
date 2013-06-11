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
 * @file           l2dbus_pendingcall.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus "pending call" object
 *===========================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_pendingcall.h"
#include "l2dbus_connection.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"
#include "l2dbus_message.h"
#include "lualib.h"

/**
 L2DBUS PendingCall

 This section describes the PendingCall object which is used to receive
 messages replies to a request. A PendingCall object cannot be created
 directly but instead is returned by a call to the Connection method
 @{l2dbus.Connection.sendWithReply|sendWithReply}.

 @namespace l2dbus.PendingCall
 */


/**
 * @brief Processes the reply message to a request.
 *
 * This function is called asynchronously from the main loop
 * whenever a reply message is received for an earlier request.
 *
 * @param [in] pending The underlying D-Bus pending call object.
 * @param [in] user User data associated with the pending call.
 */
static void
l2dbus_pendingCallHandler
    (
    DBusPendingCall*    pending,
    void*               user
    )
{
    lua_State* L = l2dbus_callbackGetThread();
    const char* errMsg = "";
    l2dbus_PendingCall* ud = l2dbus_objectRegistryGet(L, user);

    /* Nil or the PendingCall userdata is sitting at the top of the
     * stack at this point.
     */

    assert( NULL != pending );
    assert( NULL != L );

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Pending call handler invoked"));

    /* If the pending call userdata has been GC'ed then ... */
    if ( NULL == ud )
    {
        L2DBUS_TRACE((L2DBUS_TRC_WARN,
            "Cannot call handler because the pending call is GC'ed"));
    }
    else
    {
        // Push function and user value on the stack and execute the callback
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.funcRef);
        lua_pushvalue(L, -2 /* PendingCall ud */);
        lua_rawgeti(L, LUA_REGISTRYINDEX, ud->cbCtx.userRef);

        if ( 0 != lua_pcall(L, 2 /* nArgs */, 0, 0) )
        {
            if ( lua_isstring(L, -1) )
            {
                errMsg = lua_tostring(L, -1);
            }
            L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Pending call callback error: %s", errMsg));
        }
    }

    /* Clean up the thread stack */
    lua_settop(L, 0);
}


/**
 * @brief Creates a new (Lua) PendingCall object.
 *
 * This function creates a new Lua PendingCall object that
 * wraps an underlying D-Bus object.
 *
 * @param [in] L            The Lua state
 * @param [in] dbusPending  The underlying D-Bus pending call object.
 * @param [in] connIdx      A reference to the Lua connection userdata object.
 * @return Returns the Lua PendingCall userdata object on the Lua stack.
 */
int
l2dbus_newPendingCall
    (
    lua_State*              L,
    struct DBusPendingCall* dbusPending,
    int                     connIdx
    )
{
    l2dbus_PendingCall* pcUd;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: pending call"));

    connIdx = lua_absindex(L, connIdx);

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    pcUd = (l2dbus_PendingCall*)l2dbus_objectNew(L, sizeof(*pcUd),
                                             L2DBUS_PENDING_CALL_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Pending call userdata=%p", pcUd));

    if ( NULL == pcUd )
    {
        luaL_error(L, "Failed to create pending call userdata!");
    }
    else
    {
        /* Reset the userdata structure */
        l2dbus_callbackInit(&pcUd->cbCtx);
        pcUd->pendingCall = dbusPending;
        /* Add a reference to the connection userdata */
        lua_pushvalue(L, connIdx);
        pcUd->connRef = luaL_ref(L, LUA_REGISTRYINDEX);

        /* Create a weak reference to the pending call user data */
        l2dbus_objectRegistryAdd(L, pcUd, -1);
    }

    return 1;
}


/**
 * A D-Bus PendingCall class.
 * @type PendingCall
 */

/**
 @function setNotify
 @within PendingCall

 Sets a notification function for the reply.

 This method sets a notification function to be called when the reply is
 received or the pending call times out.

 @tparam userdata pending The PendingCall object
 @tparam func notifyFunc The function to be called when a reply
 message is received or a timeout occurs. The signature of the notification
 function should conform to the following:
     function onNotify(pendingCall, userToken)
 @tparam ?any userToken An optional user data value passed back to the function.
 @treturn bool Returns **true** if notification function is set, **false** otherwise.
 */
static int
l2dbus_pendingCallSetNotify
    (
    lua_State*  L
    )
{
    l2dbus_PendingCall* ud;
    int userIdx = L2DBUS_CALLBACK_NOREF_NEEDED;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    ud = (l2dbus_PendingCall*)luaL_checkudata(L, 1,
                                              L2DBUS_PENDING_CALL_MTBL_NAME);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if ( 2 < lua_gettop(L) )
    {
        /* Whatever comes after the callback function is assumed
         * to be user data
         */
        userIdx = 3;
    }

    /* Unreference the function/data associated with any *previous* callback */
    l2dbus_callbackUnref(L, &ud->cbCtx);

    /* Add references to the callback function and (optional) user data */
    l2dbus_callbackRef(L, 2, userIdx, &ud->cbCtx);

    if ( !dbus_pending_call_set_notify(ud->pendingCall,
          l2dbus_pendingCallHandler, ud, NULL) )
    {
        /* Unreference the function/data since we couldn't register
         * our notification handler
         */
        L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Failed to register pending call "
                      "notification handler"));
        l2dbus_callbackUnref(L, &ud->cbCtx);
        lua_pushboolean(L, L2DBUS_FALSE);
    }
    else
    {
        L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Registered pending call "
                              "notification handler"));
        lua_pushboolean(L, L2DBUS_TRUE);
    }

    return 1;
}


/**
 @function cancel
 @within PendingCall

 Cancels the pending call.

 This method drops the pending call such that any reply or error will
 just be ignored.

 @tparam userdata pending The PendingCall object.
 */
static int
l2dbus_pendingCallCancel
    (
    lua_State*  L
    )
{
    l2dbus_PendingCall* ud;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    ud = (l2dbus_PendingCall*)luaL_checkudata(L, 1,
                                                L2DBUS_PENDING_CALL_MTBL_NAME);

    dbus_pending_call_cancel(ud->pendingCall);
    l2dbus_callbackUnref(L, &ud->cbCtx);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Pending call cancelled"));

    return 0;
}


/**
 @function isCompleted
 @within PendingCall

 Checks whether the pending call has received a reply yet or not.

 @tparam userdata pending The PendingCall object.
 @treturn bool Returns **true** if a reply has been received and
 **false** otherwise.
 */
static int
l2dbus_pendingCallIsCompleted
    (
    lua_State*  L
    )
{
    l2dbus_PendingCall* ud;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    ud = (l2dbus_PendingCall*)luaL_checkudata(L, 1,
                                              L2DBUS_PENDING_CALL_MTBL_NAME);

    if ( dbus_pending_call_get_completed(ud->pendingCall) )
    {
        lua_pushboolean(L, L2DBUS_TRUE);
    }
    else
    {
        lua_pushboolean(L, L2DBUS_FALSE);
    }

    return 1;
}


/**
 @function stealReply
 @within PendingCall

 Tries to get the pending reply.

 The method will attempt to get any received reply and if none has
 been received yet it will return **nil**. The method can only be
 called **once** (assuming a reply message is returned) since
 ownership of the message transfers to the caller.

 @tparam userdata pending The PendingCall object.
 @treturn userdata|nil The reply message (if available) or **nil**.
 */
static int
l2dbus_pendingCallStealReply
    (
    lua_State*  L
    )
{
    l2dbus_PendingCall* ud;
    DBusMessage* msg;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    ud = (l2dbus_PendingCall*)luaL_checkudata(L, 1,
                                              L2DBUS_PENDING_CALL_MTBL_NAME);

    msg = dbus_pending_call_steal_reply(ud->pendingCall);
    if ( NULL != msg )
    {
        L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Stealing reply from pending call"));
        L2DBUS_TRACE_MSG((L2DBUS_TRC_TRACE, msg));

        /* Leaves a Message user data on the Lua stack. The
         * Lua object now owns the reference
         */
        l2dbus_messageWrap(L, msg, L2DBUS_FALSE);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}


/**
 @function block
 @within PendingCall

 Block until the pending call is completed.

 The blocking call is done as with @{l2dbus.Connection.sendWithReplyAndBlock|sendWithReplyAndBlock}
 so the call does not enter the main loop or process other messages while waiting. It simply waits
 for the reply in question. If the pending call is already completed then the method returns
 immediately.

 **Note:** When the blocking starts internally the timeout is reset so the the timeout period
 will extend beyond the time originally specified when the request was made.

 @tparam userdata pending The PendingCall object.
 */
static int
l2dbus_pendingCallBlock
    (
    lua_State*  L
    )
{
    l2dbus_PendingCall* ud;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    ud = (l2dbus_PendingCall*)luaL_checkudata(L, 1,
                                              L2DBUS_PENDING_CALL_MTBL_NAME);

    dbus_pending_call_block(ud->pendingCall);

    return 0;
}


/**
 * @brief Called by the Lua VM to GC/dispose of the PendingCall
 *
 * This function is called by the Lua VM to garbage collect the PendingCall
 * object once it is no longer referenced.
 *
 * @param [in] L            The Lua state
 * @return None
 */
static int
l2dbus_pendingCallDispose
    (
    lua_State*  L
    )
{
    l2dbus_PendingCall* ud = (l2dbus_PendingCall*)luaL_checkudata(L, 1,
                                            L2DBUS_PENDING_CALL_MTBL_NAME);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: pending call (userdata=%p)", ud));

    if ( !dbus_pending_call_get_completed(ud->pendingCall) )
    {
        dbus_pending_call_cancel(ud->pendingCall);
    }

    if ( ud->pendingCall != NULL )
    {
        dbus_pending_call_unref(ud->pendingCall);
    }

    /* Drop the weak reference to the userdata */
    l2dbus_objectRegistryRemove(L, ud);

    /* We no longer need to anchor the dispatcher */
    luaL_unref(L, LUA_REGISTRYINDEX, ud->connRef);

    /* Unreference the function/data associated with a callback */
    l2dbus_callbackUnref(L, &ud->cbCtx);

    return 0;
}


/*
 * Define the methods of the PendingCall class
 */
static const luaL_Reg l2dbus_pendingCallMetaTable[] = {
    {"setNotify", l2dbus_pendingCallSetNotify},
    {"cancel", l2dbus_pendingCallCancel},
    {"isCompleted", l2dbus_pendingCallIsCompleted},
    {"stealReply", l2dbus_pendingCallStealReply},
    {"block", l2dbus_pendingCallBlock},
    {"__gc", l2dbus_pendingCallDispose},
    {NULL, NULL},
};


/**
 * @brief "Opens" the PendingCall sub-module.
 *
 * This function creates a metatable entry for the PendingCall userdata
 * and simulates opening the PendingCall sub-module. The corresponding
 * object can **only** be created by a Connection:sendWithReply call.
 *
 * @return None
 *
 */
void
l2dbus_openPendingCall
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_PENDING_CALL_TYPE_ID,
            l2dbus_pendingCallMetaTable));
}



