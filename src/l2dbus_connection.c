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

/**
 L2DBUS Connection

 This module defines the functions/methods associated with the L2DBUS Connection
 class.

 @module l2dbus.Connection
 */


/**
 @function open

 Opens a connection to a remote address.

 This function opens and returns a D-Bus connection. It can be used to open an
 arbitrary address instead of one of the "standard" well-known ones. Optionally
 the connection can be opened shared (e.g. if the connection already exists then
 the existing connection is returned) or private. Likewise the connection
 can be configured to cause the application to exit on disconnect. By default
 the connection will be shared and will **not** exit on disconnect. It's recommended
 that all connections should be opened shared to minimize resource usage.

 When the connection is no longer referenced the Lua garbage collector will either
 close the connection (if opened privately) or unreference the connection (if opened as shared).

 @tparam userdata dispatcher The @{l2dbus.Dispatcher|Dispatcher} to associate with the
 connection.
 @tparam string address Remote D-Bus address to open
 @tparam ?bool privateConn Optional flag indicating whether this
 is a private (**true**) or shared (**false**) connection. Defaults to a shared connection (**false**)
 @tparam ?bool exitOnDisconnect Optional flag indicating whether
 the application should exit on disconnect (**true** or **false**). Defaults to **false**.
 @treturn userdata Connection userdata object
 */
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


/**
 @function openStandard

 Opens a connection to one of the standard well-known buses.

 This function opens and returns a D-Bus connection to one of the standard well-known buses.
 The well-known buses include the @{l2dbus.Dbus.BUS_SYSTEM|BUS_SYSTEM},
 @{l2dbus.Dbus.BUS_SESSION|BUS_SESSION}, and @{l2dbus.Dbus.BUS_STARTER|BUS_STARTER} buses.
 The connection can optionally be opened shared (e.g. if the connection already exists then
 the existing connection is returned) or private. Likewise the connection
 can be configured to cause the application to exit on disconnect. By default
 the connection will be shared and will **not** exit on disconnect. It's recommended
 that all connections should be opened shared to minimize resource usage.

 When the connection is no longer referenced the Lua garbage collector will either
 close the connection (if opened privately) or unreference the connection (if opened as shared).

 @tparam userdata dispatcher The @{l2dbus.Dispatcher|Dispatcher} to associate with the
 connection.
 @tparam number busId The constant bus identifier: @{l2dbus.Dbus.BUS_SYSTEM|BUS_SYSTEM},
 @{l2dbus.Dbus.BUS_SESSION|BUS_SESSION}, or  @{l2dbus.Dbus.BUS_STARTER|BUS_STARTER}
 @tparam ?bool privateConn Optional flag indicating whether this
 is a private (**true**) or shared (**false**) connection. Defaults to a shared connection (**false**)
 @tparam ?bool exitOnDisconnect Optional flag indicating whether
 the application should exit on disconnect (**true** or **false**). Defaults to **false**.
 @treturn userdata Connection userdata object
 */
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

/**
 * A D-Bus Connection class.
 * @type Connection
 */

/**
 @function isConnected
 @within Connection

 Tests whether the connection is connected to the bus.

 This method can be called to detect whether or not
 the provided connection is in fact connected to the
 bus.

 @tparam userdata conn The D-Bus connection object
 @treturn bool Returns **true** if connected, **false** otherwise.
 */
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


/**
 @function isAuthenticated
 @within Connection

 Tests whether the connection has been authenticated.

 This method can be called to detect whether or not
 the provided connection has been authenticated.

 **Note:** If the connection was authenticated then disconnected then
 this function still returns **true**.

 @tparam userdata conn The D-Bus connection object
 @treturn bool Returns **true** if authenticated, **false** otherwise.
 */
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


/**
 @function isAnonymous
 @within Connection

 Tests whether the connection is not authenticated as a specific user.

 If the connection is not authenticated, this function returns **true**,
 and if it is authenticated but as an anonymous user, it returns **true**.
 If it is authenticated as a specific user, then this returns **false**.

 **Note:**  If the connection was authenticated as anonymous then
 disconnected, this function still returns **true**.


 @tparam userdata conn The D-Bus connection object
 @treturn bool Returns **true** if not authenticated or authenticated as anonymous
 */
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


/**
 @function getServerId
 @within Connection

 Gets the ID of the server address we are authenticated to on a client-side connection.

 If the connection is on the server side this will always return nil. For additional
 details on this method see the original 'C' documentation
 <a href="http://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html#gae6c19e146a37f9de6a06c1617874bed9">here</a>.

 @tparam userdata conn The D-Bus connection object
 @treturn string|nil Returns the ID of the server if a client-side connection, **nil** otherwise.
 */
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


/**
 @function getBusId
 @within Connection

 Asks the bus to return its the globally unique ID as described in the D-Bus specification.

 For additional details on this method see the original 'C' documentation
 <a href="http://dbus.freedesktop.org/doc/api/html/group__DBusBus.html#ga18314500e7f6890a79bddbeace5df5f9">here</a>.

 @tparam userdata conn The D-Bus connection object
 @treturn string|nil Returns bus ID or **nil** if there is an error.
 */
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


/**
 @function getDescriptor
 @within Connection

 Retrieves the underlying file descriptor for the connection.

 If the underlying file descriptor is not available then *nil* will be
 returned. **DO NOT** read or write to this file descriptor or try to
 *select()* on it if already utilizing a D-Bus @{l2dbus.Watch|Watch} for
 main loop integration. Exceptions *may* exist if attempting to integrate
 external main loops with the L2DBUS @{l2dbus.Dispatcher|Dispatcher}. In this
 scenario the @{l2dbus.Dispatcher.run|run} method should **only** be called
 when the external loop detects write activity on this descriptor. The
 @{l2dbus.Dispatcher.run|run} method should be called with the
 @{l2dbus.Dispatcher.DISPATCH_NO_WAIT} option followed by a call to
 @{flush|Connection:flush} to dispatch any queued messages.

 @tparam userdata conn The D-Bus connection object.
 @treturn number|nil Returns the file descriptor or **nil** if none is
 available.
 */
static int
l2dbus_connectionGetDescriptor
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    cdbus_Descriptor descr;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    if ( cdbus_connectionGetDescriptor(connUd->conn, &descr) )
    {
        lua_pushinteger(L, descr);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}


/**
 @function canSendType
 @within Connection

 Tests whether a certain type can be send via the connection.

 This function will always return **true** for all types with the exception of DBUS_TYPE_UNIX_FD.
 For additional details on this method see the original 'C' documentation
 <a href="http://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html#ga3e41509b3afdbc22872bacc5754e85c2">here</a>.

 @tparam userdata conn The D-Bus connection object
 @tparam number|string type The D-Bus type to check. The type definitions can be found
 <a href="http://dbus.freedesktop.org/doc/api/html/group__DBusProtocol.html">here</a>. The
 type can be expressed as a D-Bus integral type or as a single character string.
 @treturn bool Returns **true** if the D-Bus type can be sent or **false** otherwise.
 */
static int
l2dbus_connectionCanSendType
    (
    lua_State*  L
    )
{
    l2dbus_Connection* connUd;
    int dbusType;
    const char* typeStr;
    size_t len;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    connUd = (l2dbus_Connection*)luaL_checkudata(L, 1,
                                                L2DBUS_CONNECTION_MTBL_NAME);

    if ( LUA_TNUMBER == lua_type(L, 2) )
    {
        dbusType = lua_tointeger(L, 2);
    }
    else if ( LUA_TSTRING == lua_type(L, 2) )
    {
        typeStr = lua_tolstring(L, 2, &len);
        if ( len > 0 )
        {
            dbusType = (int)typeStr[0];
        }
        else
        {
            luaL_argerror(L, 2, "expected a D-Bus type encoded as a single character string");
        }
    }
    else
    {
        luaL_argerror(L, 2, "expected a D-Bus type encoded as an integer or single character string");
    }

    lua_pushboolean(L, dbus_connection_can_send_type(
                    cdbus_connectionGetDBus(connUd->conn), dbusType));

    return 1;
}


/**
 @function send
 @within Connection

 Adds a message to the outgoing message queue.

 This method does **not** block to write the message to the network;
 that happens asynchronously. To force the message to be written an explicit
 call to @{flush} must be made otherwise the message will be sent the next
 time the main loop is run.

 @tparam userdata conn The D-Bus connection object
 @tparam userdata msg The D-Bus message to send
 @treturn bool Returns **true** if the message is queued to be sent and
 **false** otherwise.
 @treturn number If the message is queued successfully then this will be
 the transmitting serial number of the message otherwise this is zero (0).
 */
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


/**
 @function sendWithReply
 @within Connection

 Queues a message to send as with @{send} but returns a PendingCall to
 receive the reply message later.

 If no reply is received in the given (optional) timeout the @{l2dbus.PendingCall|PendingCall}
 expires and a synthetic error reply (generated locally, not by the remote
 application) is generated indicating a timeout occurred.

 @tparam userdata conn The D-Bus connection object
 @tparam userdata msg The D-Bus message to send
 @tparam ?number timeout An optional timeout in milliseconds to wait for a
 reply. Two special values are allowed as well:
 @{l2dbus.Dbus.TIMEOUT_USE_DEFAULT|TIMEOUT_USE_DEFAULT}
 and @{l2dbus.Dbus.TIMEOUT_INFINITE|TIMEOUT_INFINITE}. The default
 value (if none is specified) is @{l2dbus.Dbus.TIMEOUT_USE_DEFAULT|TIMEOUT_USE_DEFAULT}.
 @treturn bool Returns **true** if the message is queued to be sent
 and **false** otherwise.
 @treturn userdata|nil If the message is queued successfully then a
 @{l2dbus.PendingCall|PendingCall} is returned or **nil** on failure.
 */
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


/**
 @function sendWithReplyAndBlock
 @within Connection

 Sends a message and blocks for a certain period of time while waiting
 for a reply.

 This function does **not** re-enter the main loop. This means messages
 other than the reply (e.g. other requests, signals, etc...) are queued
 but not processed. This function is used to invoke method calls on a
 remote object. If a normal reply is received then the reply message is
 returned. If an error is encountered or a D-Bus error message is received
 then **nil** is returned as the first return parameter followed by an optional
 error name and error message.

 @tparam userdata conn The D-Bus connection object
 @tparam userdata msg The D-Bus message to send
 @tparam ?number timeout An optional timeout in milliseconds to wait for a
 reply. Two special values are allowed as well:
 @{l2dbus.Dbus.TIMEOUT_USE_DEFAULT|TIMEOUT_USE_DEFAULT}
 and @{l2dbus.Dbus.TIMEOUT_INFINITE|TIMEOUT_INFINITE}. The default
 value (if none is specified) is @{l2dbus.Dbus.TIMEOUT_USE_DEFAULT|TIMEOUT_USE_DEFAULT}.
 @treturn userdata|nil The reply message or **nil** if there was an error. If
 a D-Bus error message is received then **nil** is returned here and the
 error name and message are returned in the next two parameters.
 @treturn string|nil If there was an error **or** a D-Bus error message was
 returned then a string indicating the error name *may* be returned or **nil**
 @treturn string|nil If there was an error **or** a D-Bus error message was
 returned then a string containing an optional error message *may* be returned
 or **nil** (if there is no message).
 */
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


/**
 @function flush
 @within Connection

 Blocks until the outgoing message queue is empty.

 @tparam userdata conn The D-Bus connection object
 */
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


/**
 @function hasMessagesToSend
 @within Connection

 Checks whether there are messages in the outgoing message queue.

 The @{flush} method can be used to block until all outgoing messages
 have been written to the underlying transport (such as a socket).

 @tparam userdata conn The D-Bus connection object
 @treturn bool Returns **true** if there are pending messages in
 the outgoing queue or **false** otherwise.
 */
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


/**
 @function registerMatch
 @within Connection

 Registers a handler for messages that match a defined set of rules.

 This method registers a function to receive messages that match a
 specific *match* rule. A match rule as described
 <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-bus-routing-match-rules">here</a>
 is part of the message bus routing protocol. These rules describe the
 messages that should be delivered to a client based on the contents of
 a message. The rule itself is describe by a Lua @{l2dbus.Match.MatchRule|table}
 that contains specific fields that map to keywords in the D-Bus specification.

 The message handler should have the following prototype:
     function onMatch(match, message, userToken)

 @tparam userdata conn The D-Bus connection object
 @tparam table rule The match rule table
 @see l2dbus.Match.MatchRule
 @tparam func handler The message handler for the rule
 @tparam ?any userToken Optional user defined data that is delivered to
 the message handler
 @treturn lightuserdata Returns a *match* rule handle. The handle can be used
 to @{unregisterMatch|unregister} a match.
 */
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


/**
 @function unregisterMatch
 @within Connection

 Unregisters a message handler for the given match rule.

 This method unregisters the handler for the match rule referenced by the
 handle returned by the call to @{registerMatch|register} a match.

 @tparam userdata conn The D-Bus connection object
 @tparam lightuserdata handle The match rule handle
 @treturn bool Returns **true** if the match rule is unregistered successfully
 and **false** if the handle is unknown or there is an error.
 */
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


/**
 @function registerServiceObject
 @within Connection

 Registers a @{l2dbus.ServiceObject|service object} with the connection.

 This method registers a service object with the connection that
 will receive and handle requests from clients. A service object
 can be registered with more than one connection if necessary. This means
 a service object can span multiple buses (e.g. the same instance across a
 *session* or *system* bus).

 @tparam userdata conn The D-Bus connection object
 @tparam userdata svcObj The D-Bus service object
 @treturn bool Returns **true** if the service object is registered
 successfully and **false** otherwise
 */
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


/**
 @function unregisterServiceObject
 @within Connection

 Unregisters a @{l2dbus.ServiceObject|service object} from the connection.

 This method unregisters a @{l2dbus.ServiceObject|service object} from the
 specified D-Bus connection.

 @tparam userdata conn The D-Bus connection object
 @tparam userdata svcObj The D-Bus service object to unregister
 @treturn bool Returns **true** if the service object is unregistered
 successfully and **false** otherwise
 */
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


/**
 * @brief Called by Lua VM to GC/reclaim the Connection userdata.
 *
 * This method is called by the Lua VM to reclaim the Connection
 * userdata.
 *
 * @return nil
 *
 */
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


/*
 * Define the methods of the Connection class
 */
static const luaL_Reg l2dbus_connMetaTable[] = {
    {"isConnected", l2dbus_connectionIsConnected},
    {"isAuthenticated", l2dbus_connectionIsAuthenticated},
    {"isAnonymous", l2dbus_connectionIsAnonymous},
    {"getServerId", l2dbus_connectionGetServerId},
    {"getBusId", l2dbus_connectionGetBusId},
    {"getDescriptor", l2dbus_connectionGetDescriptor},
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


/**
 * @brief Creates the Connection sub-module.
 *
 * This function creates a metatable entry for the Connection userdata
 * and simulates opening the Connection sub-module.
 *
 * @return A table defining the Connection sub-module
 *
 */
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
