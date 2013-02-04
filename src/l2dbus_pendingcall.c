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
 * @file           l2dbus_pendingcall.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus "pending call" object
 *******************************************************************************
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


static void
l2dbus_pendingCallHandler
    (
    DBusPendingCall*    pending,
    void*               user
    )
{
    lua_State* L = l2dbus_callbackGetThread();
    const char* errMsg = "";
    l2dbus_PendingCall* ud = l2dbus_callbackFetchUd(L, user);

    /* Nil or the PendingCall userdata is sitting at the top of the
     * stack at this point.
     */

    assert( NULL != pending );
    assert( NULL != L );

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
        l2dbus_callbackUnref(L, &ud->cbCtx);
        lua_pushboolean(L, L2DBUS_FALSE);
    }
    else
    {
        lua_pushboolean(L, L2DBUS_TRUE);
    }

    return 1;
}


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

    return 0;
}


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
        /* Leaves a Message user data on the Lua stack. The
         * Lua object now owns the reference
         */
        l2dbus_messageWrap(L, msg, L2DBUS_TRUE);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}


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



static const luaL_Reg l2dbus_pendingCallMetaTable[] = {
    {"setNotify", l2dbus_pendingCallSetNotify},
    {"cancel", l2dbus_pendingCallCancel},
    {"isCompleted", l2dbus_pendingCallIsCompleted},
    {"stealReply", l2dbus_pendingCallStealReply},
    {"block", l2dbus_pendingCallBlock},
    {"__gc", l2dbus_pendingCallDispose},
    {NULL, NULL},
};


void
l2dbus_openPendingCall
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_PENDING_CALL_TYPE_ID,
            l2dbus_pendingCallMetaTable));
}



