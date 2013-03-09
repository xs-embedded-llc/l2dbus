/*===========================================================================
 *
 * Project         l2dbus
 * (c) Copyright   2013 XS-Embedded LLC
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
 *===========================================================================
 *===========================================================================
 * @file           l2dbus_connection.c        
 * @author         Glenn Schmottlach
 * @brief          Implementation of the Lua interface to a D-Bus connection.
 *===========================================================================
 */
#include <stdlib.h>
#include <assert.h>
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_connection.h"
#include "l2dbus_dispatcher.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"
#include "l2dbus_message.h"
#include "l2dbus_pendingcall.h"
#include "l2dbus_match.h"
#include "l2dbus_serviceobject.h"


static int
l2dbus_openConnection
    (
    lua_State*  L
    )
{
    l2dbus_Dispatcher* dispUd;
    l2dbus_Connection* connUd;
    int nArgs = 0;
    cdbus_Bool privConn = CDBUS_FALSE;
    cdbus_Bool exitOnDisconnect = CDBUS_FALSE;
    const char* address = NULL;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: connection"));

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    /* See how many arguments were passed in */
    nArgs = lua_gettop(L);

    if ( nArgs <  2 )
    {
        luaL_error(L, "Insufficient number of parameters");
    }

    dispUd = (l2dbus_Dispatcher*)luaL_checkudata(L, 1,
                                L2DBUS_DISPATCHER_MTBL_NAME);

    address = luaL_checkstring(L, 2);


    if ( nArgs >= 3 )
    {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        privConn = lua_toboolean(L, 3) ? CDBUS_TRUE : CDBUS_FALSE;
    }

    if ( nArgs >= 4 )
    {
        luaL_checktype(L, 4, LUA_TBOOLEAN);
        exitOnDisconnect = lua_toboolean(L, 4) ? CDBUS_TRUE : CDBUS_FALSE;
    }

    connUd = (l2dbus_Connection*)l2dbus_objectNew(L, sizeof(*connUd),
                                             L2DBUS_CONNECTION_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Connection userdata=%p", connUd));

    if ( NULL == connUd )
    {
        luaL_error(L, "Failed to create connection userdata!");
    }
    else
    {
        /* Reset the userdata structure */
        LIST_INIT(&connUd->matches);
        connUd->dispUdRef = LUA_NOREF;

        connUd->conn = cdbus_connectionOpen(dispUd->disp, address,
                                            privConn, exitOnDisconnect);

        if ( NULL == connUd->conn )
        {
            luaL_error(L, "Failed to allocate Connection");
        }
        else
        {
            /* Add a reference to the Dispatcher userdata */
            lua_pushvalue(L, 1 /* dispUd */);
            connUd->dispUdRef = luaL_ref(L, LUA_REGISTRYINDEX);

            /* Add a (weak) mapping between the CDBUS connection and
             * the associated Lua userdata wrapper
             */
            l2dbus_objectRegistryAdd(L, connUd->conn, -1);
        }
    }

    return 1;
}


static int
l2dbus_openStandardConnection
    (
    lua_State*  L
    )
{
    l2dbus_Dispatcher* dispUd;
    l2dbus_Connection* connUd;
    int nArgs = 0;
    cdbus_Bool privConn = CDBUS_FALSE;
    cdbus_Bool exitOnDisconnect = CDBUS_FALSE;
    DBusBusType busType = DBUS_BUS_SESSION;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: connection"));

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    /* See how many arguments were passed in */
    nArgs = lua_gettop(L);

    if ( nArgs <  2 )
    {
        luaL_error(L, "Insufficient number of parameters");
    }

    dispUd = (l2dbus_Dispatcher*)luaL_checkudata(L, 1,
                                L2DBUS_DISPATCHER_MTBL_NAME);

    busType = (DBusBusType)luaL_checkint(L, 2);


    if ( nArgs >= 3 )
    {
        luaL_checktype(L, 3, LUA_TBOOLEAN);
        privConn = lua_toboolean(L, 3) ? CDBUS_TRUE : CDBUS_FALSE;
    }

    if ( nArgs >= 4 )
    {
        luaL_checktype(L, 4, LUA_TBOOLEAN);
        exitOnDisconnect = lua_toboolean(L, 4) ? CDBUS_TRUE : CDBUS_FALSE;
    }

    connUd = (l2dbus_Connection*)l2dbus_objectNew(L, sizeof(*connUd),
                                             L2DBUS_CONNECTION_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Connection userdata=%p", connUd));

    if ( NULL == connUd )
    {
        luaL_error(L, "Failed to create connection userdata!");
    }
    else
    {
        /* Reset the userdata structure */
        LIST_INIT(&connUd->matches);
        connUd->dispUdRef = LUA_NOREF;

        connUd->conn = cdbus_connectionOpenStandard(dispUd->disp, busType,
                                            privConn, exitOnDisconnect);

        if ( NULL == connUd->conn )
        {
            luaL_error(L, "Failed to allocate Connection");
        }
        else
        {
            /* Add a reference to the Dispatcher userdata */
            lua_pushvalue(L, 1 /* dispUd */);
            connUd->dispUdRef = luaL_ref(L, LUA_REGISTRYINDEX);

            /* Add a (weak) mapping between the CDBUS connection and
             * the associated Lua userdata wrapper
             */
            l2dbus_objectRegistryAdd(L, connUd->conn, -1);
        }
    }

    return 1;
}

static int
l2dbus_connectionIsConnected
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    lua_pushboolean(L, dbus_connection_get_is_connected(
                    cdbus_connectionGetDBus(connUd->conn)));

    return 1;
}


static int
l2dbus_connectionIsAuthenticated
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    lua_pushboolean(L, dbus_connection_get_is_authenticated(
                    cdbus_connectionGetDBus(connUd->conn)));

    return 1;
}


static int
l2dbus_connectionIsAnonymous
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    lua_pushboolean(L, dbus_connection_get_is_anonymous(
                    cdbus_connectionGetDBus(connUd->conn)));

    return 1;
}


static int
l2dbus_connectionGetServerId
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    char* serverId;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    serverId = dbus_connection_get_server_id(
                    cdbus_connectionGetDBus(connUd->conn));
    if ( NULL == serverId )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, serverId);
        dbus_free(serverId);
    }

    return 1;
}


static int
l2dbus_connectionGetBusId
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    char* busId;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    busId = dbus_bus_get_id(
                    cdbus_connectionGetDBus(connUd->conn), NULL);
    if ( NULL == busId )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, busId);
        dbus_free(busId);
    }

    return 1;
}


static int
l2dbus_connectionCanSendType
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    int dbusType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    dbusType = luaL_checkinteger(L, 2);

    lua_pushboolean(L, dbus_connection_can_send_type(
                    cdbus_connectionGetDBus(connUd->conn), dbusType));

    return 1;
}

static int
l2dbus_connectionSend
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    l2dbus_Message* msgUd;
    dbus_uint32_t serialNum = 0;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                            L2DBUS_CONNECTION_MTBL_NAME);
    msgUd = (l2dbus_Message*)luaL_checkudata(L, 2, L2DBUS_MESSAGE_MTBL_NAME);

    lua_pushboolean(L, dbus_connection_send(cdbus_connectionGetDBus(connUd->conn),
                    msgUd->msg, &serialNum));
    lua_pushnumber(L, serialNum);

    return 2;
}


static int
l2dbus_connectionSendWithReply
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    l2dbus_Message* msgUd;
    int msecTimeout;
    DBusPendingCall* pending = NULL;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                            L2DBUS_CONNECTION_MTBL_NAME);
    msgUd = (l2dbus_Message*)luaL_checkudata(L, 2, L2DBUS_MESSAGE_MTBL_NAME);
    msecTimeout = luaL_optint(L, 3, DBUS_TIMEOUT_USE_DEFAULT);

    if ( dbus_connection_send_with_reply(cdbus_connectionGetDBus(connUd->conn),
        msgUd->msg, &pending, msecTimeout) && (NULL != pending) )
    {
        lua_pushboolean(L, L2DBUS_TRUE);
        l2dbus_newPendingCall(L, pending, 1);
    }
    else
    {
        lua_pushboolean(L, L2DBUS_FALSE);
        lua_pushnil(L);
    }
    return 2;
}


static int
l2dbus_connectionSendWithReplyAndBlock
    (
    lua_State*  L
    )
{
    DBusError dbusError;
    DBusMessage* replyMsg;
    l2dbus_Message* replyMsgUd;
    l2dbus_Connection* connUd;
    l2dbus_Message* msgUd;
    int msecTimeout;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                            L2DBUS_CONNECTION_MTBL_NAME);
    msgUd = (l2dbus_Message*)luaL_checkudata(L, 2, L2DBUS_MESSAGE_MTBL_NAME);
    msecTimeout = luaL_optint(L, 3, DBUS_TIMEOUT_USE_DEFAULT);

    dbus_error_init(&dbusError);

    replyMsg = dbus_connection_send_with_reply_and_block(cdbus_connectionGetDBus(connUd->conn),
                                                        msgUd->msg, msecTimeout, &dbusError);

    if ( NULL == replyMsg )
    {
        lua_pushnil(L);
        lua_pushstring(L, (NULL != dbusError.name) ? dbusError.name : DBUS_ERROR_FAILED);
        lua_pushstring(L, (NULL != dbusError.message) ? dbusError.message : "");
    }
    else
    {
        /* Leave the userdata on the Lua stack if successful */
        replyMsgUd = l2dbus_messageWrap(L, replyMsg, L2DBUS_FALSE);
        if ( NULL == replyMsgUd )
        {
            L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Failed to wrap D-Bus reply message (serial #=%d)",
                            dbus_message_get_serial(replyMsg)));
            dbus_message_unref(replyMsg);
            lua_pushstring(L, DBUS_ERROR_NO_MEMORY);
            lua_pushstring(L, "Failed to bind to reply message");
        }
        else
        {
            /* The Message user data it already sitting on the stack */

            /* No error name */
            lua_pushnil(L);
            /* No message */
            lua_pushnil(L);
        }
    }
    dbus_error_free(&dbusError);

    /* Message, error name (if any), message (if any) */
    return 3;
}


static int
l2dbus_connectionFlush
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);
    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);
    dbus_connection_flush(cdbus_connectionGetDBus(connUd->conn));

    return 0;
}


static int
l2dbus_connectionHasMessagesToSend
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);
    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);
    if ( dbus_connection_has_messages_to_send(
        cdbus_connectionGetDBus(connUd->conn)) )
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
l2dbus_connectionRegisterMatch
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    l2dbus_Match* match;
    int userIdx = L2DBUS_CALLBACK_NOREF_NEEDED;
    const char* errReason;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);
    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);
    luaL_checktype(L, 2, LUA_TTABLE);

    luaL_checktype(L, 3, LUA_TFUNCTION);

    if ( 3 < lua_gettop(L) )
    {
        /* Whatever comes after the callback function is assumed
         * to be user data
         */
        userIdx = 4;
    }

    match = l2dbus_newMatch(L, 2 /*rules*/, 3 /*callback*/, userIdx, 1 /*conn*/,
                            &errReason);

    if ( NULL == match )
    {
        luaL_error(L, errReason);
    }

    LIST_INSERT_HEAD(&connUd->matches, match, link);

    lua_pushlightuserdata(L, match);

    return 1;
}


static int
l2dbus_connectionUnregisterMatch
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    l2dbus_Match* match;
    l2dbus_Match* hnd;
    l2dbus_Bool isUnregistered = L2DBUS_TRUE;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);
    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    luaL_checktype(L, 2, LUA_TLIGHTUSERDATA);
    hnd = (l2dbus_Match*)lua_touserdata(L, 2);

    /* Need to find and then unregister the match */
    /* Find the registered match and remove it */
    LIST_FOREACH(match, &connUd->matches, link)
    {
        if ( (l2dbus_Match*)hnd == match )
        {
            LIST_REMOVE(match, link);
            l2dbus_disposeMatch(L, match);
            isUnregistered = L2DBUS_TRUE;
            break;
        }
    }

    lua_pushboolean(L, isUnregistered);

    return 1;
}


static int
l2dbus_connectionRegisterObject
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    l2dbus_ServiceObject* svcObjUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);
    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);
    svcObjUd = (l2dbus_ServiceObject*)luaL_checkudata(L, 2,
                                                L2DBUS_SERVICE_OBJECT_MTBL_NAME);

    lua_pushboolean(L, cdbus_connectionRegisterObject(connUd->conn, svcObjUd->obj));

    return 1;
}


static int
l2dbus_connectionUnregisterObject
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    l2dbus_ServiceObject* svcObjUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);
    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    svcObjUd = (l2dbus_ServiceObject*)luaL_checkudata(L, 2,
                                                L2DBUS_SERVICE_OBJECT_MTBL_NAME);
    lua_pushboolean(L, cdbus_connectionUnregisterObject(connUd->conn,
                       cdbus_objectGetPath(svcObjUd->obj)));

    return 1;
}


static int
l2dbus_connectionDispose
    (
    lua_State*  L
    )
{
    cdbus_HResult rc;
    l2dbus_Match* match;
    l2dbus_Match* next;

    l2dbus_Connection* ud = (l2dbus_Connection*)luaL_checkudata(L, -1,
                                        L2DBUS_CONNECTION_MTBL_NAME);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: connection (userdata=%p)", ud));

    /* Loop through any matches we have and dispose of them */
    for ( match = LIST_FIRST(&ud->matches);
        match != LIST_END(&ud->matches);
        match = next )
    {
        next = LIST_NEXT(match, link);
        l2dbus_disposeMatch(L, match);
    }

    if ( ud->conn != NULL )
    {
        /* Remove the (weak) association between
         * the CDBUS connection and the Lua userdata
         * wrapper.
         */
        l2dbus_objectRegistryRemove(L, ud->conn);

        rc = cdbus_connectionClose(ud->conn);
        if ( CDBUS_FAILED(rc) )
        {
            L2DBUS_TRACE((L2DBUS_TRC_WARN, "Failed to close connection (0x%X)", rc));
        }
        cdbus_connectionUnref(ud->conn);
    }

    /* Drop the weak reference to the userdata */
    l2dbus_objectRegistryRemove(L, ud);

    /* We no longer need to anchor the dispatcher */
    luaL_unref(L, LUA_REGISTRYINDEX, ud->dispUdRef);

    /* Unreference and function/data associated with a callback */
    l2dbus_callbackUnref(L, &ud->cbCtx);

    return 0;
}


static const luaL_Reg l2dbus_connMetaTable[] = {
    {"isConnected", l2dbus_connectionIsConnected},
    {"isAuthenticated", l2dbus_connectionIsAuthenticated},
    {"isAnonymous", l2dbus_connectionIsAnonymous},
    {"getServerId", l2dbus_connectionGetServerId},
    {"getBusId", l2dbus_connectionGetBusId},
    {"canSendType", l2dbus_connectionCanSendType},
    {"flush", l2dbus_connectionFlush},
    {"hasMessagesToSend", l2dbus_connectionHasMessagesToSend},
    {"send", l2dbus_connectionSend},
    {"sendWithReply", l2dbus_connectionSendWithReply},
    {"sendWithReplyAndBlock", l2dbus_connectionSendWithReplyAndBlock},
    {"registerMatch", l2dbus_connectionRegisterMatch},
    {"unregisterMatch", l2dbus_connectionUnregisterMatch},
    {"registerServiceObject", l2dbus_connectionRegisterObject},
    {"unregisterServiceObject", l2dbus_connectionUnregisterObject},
    {"__gc", l2dbus_connectionDispose},
    {NULL, NULL},
};


void
l2dbus_openConnectionLib
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_CONNECTION_TYPE_ID, l2dbus_connMetaTable));
    lua_createtable(L, 0, 2);
    lua_pushcfunction(L, l2dbus_openConnection);
    lua_setfield(L, -2, "open");

    lua_pushcfunction(L, l2dbus_openStandardConnection);
    lua_setfield(L, -2, "openStandard");
}
