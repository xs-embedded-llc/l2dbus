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
 * @file           l2dbus_message.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus message object wrapper.
 *===========================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "dbus/dbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"
#include "l2dbus_message.h"
#include "l2dbus_transcode.h"
#include "l2dbus_alloc.h"
#include "l2dbus_dbuscompat.h"
#include "lauxlib.h"


/**
 L2DBUS Message

 This section describes the Message class which is used to encapsulate D-Bus
 messages in Lua.

 @namespace l2dbus.Message
 */


/**
 @function new

 Creates a new D-Bus Message of the specified type.

 Constructs one of the following types of new D-Bus messages:
 <ul>
 <li>@{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL} or @{METHOD_CALL}</li>
 <li>@{l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN|MESSAGE_TYPE_METHOD_RETURN} or @{METHOD_RETURN}</li>
 <li>@{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL} or @{SIGNAL}</li>
 <li>@{l2dbus.Dbus.MESSAGE_TYPE_ERROR|MESSAGE_TYPE_ERROR} or @{ERROR}</li>
 </ul>
 The constructed message is initially empty (e.g. there are no arguments or
 parameters assigned to it).

 @tparam number msgType One of the defined D-Bus message types:
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL},
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN|MESSAGE_TYPE_METHOD_RETURN},
 @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL}, or
 @{l2dbus.Dbus.MESSAGE_TYPE_ERROR|MESSAGE_TYPE_ERROR}
 @treturn userdata Message userdata object
 */
static int
l2dbus_newMessage
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;
    int dbusMsgType;
    DBusMessage* dbusMsg;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: message"));

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    dbusMsgType = luaL_checkinteger(L, 1);

    switch ( dbusMsgType )
    {
        case DBUS_MESSAGE_TYPE_METHOD_CALL:
        case DBUS_MESSAGE_TYPE_METHOD_RETURN:
        case DBUS_MESSAGE_TYPE_SIGNAL:
        case DBUS_MESSAGE_TYPE_ERROR:
            break;

        default:
            luaL_error(L, "unsupported message type (=> %d)", dbusMsgType);
            break;
    }

    dbusMsg = dbus_message_new(dbusMsgType);
    if ( NULL == dbusMsg )
    {
        luaL_error(L, "failed to allocate D-Bus message");
    }

    msgUd = (l2dbus_Message*)l2dbus_objectNew(L, sizeof(*msgUd),
                                                 L2DBUS_MESSAGE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Message userdata=%p", msgUd));

    if ( NULL == msgUd )
    {
        dbus_message_unref(dbusMsg);
        luaL_error(L, "failed to allocate userdata for DBus message");
    }
    else
    {
        msgUd->msg = dbusMsg;
    }

    return 1;
}


/**
 A D-Bus method call message "initializer" table.

 This table can be used in lieu of a parameter list to initialize a new
 D-Bus method call message.

 @see newMethodCall

 @table MethodCallInitializer
 @field method (string) [Req] The method name to call.
 @field interface (string) [Opt] The interface to invoke the method on.
 @field path (string) [Req] The D-Bus object path the message should be sent to.
 @field destination (string) [Opt] The bus name that the message should be sent to.
 */


/**
 @function newMethodCall

 Creates a new D-Bus method call Message.

 Constructs a D-Bus method call message either using an initialization
 @{MethodCallInitializer|table} or a parameter list. The *destination* may
 be **nil** in which no destination is set which is appropriate when
 using D-Bus in a peer-to-peer context (e.g. no message bus). The
 *interface* may be **nil** which means it remains undefined which
 object receives the message if more than one interface defines the
 same member name.

 @tparam string|table member The member name to call **or** a
 @{MethodCallInitializer} table containing all the method call parameters. If
 a table is specified then the parameters following the table are ignored.
 @tparam ?string|nil interface The interface to invoke the method on. Ignored
 if the first parameter is a table.
 @tparam string path The D-Bus object path the message should be sent to. Ignored
 if the first parameter is a table.
 @tparam ?string|nil destination The bus name that the message should be sent to.
 Ignored if the first parameter is a table.
 @treturn userdata Message userdata object
 */
static int
l2dbus_newMessageMethodCall
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;
    DBusMessage* dbusMsg;
    const char* destination = NULL;
    const char* path = NULL;
    const char* interface = NULL;
    const char* method = NULL;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    if ( LUA_TTABLE == lua_type(L, 1) )
    {
        luaL_checkstack(L, 4, "cannot grow Lua stack to parse arguments");

        lua_getfield(L, 1, "destination");
        if ( lua_isstring(L, -1) )
        {
            destination = lua_tostring(L, -1);
        }

        lua_getfield(L, 1, "path");
        if ( !lua_isstring(L, -1) )
        {
            luaL_error(L, "expecting 'path' field with string value");
        }
        else
        {
            path = lua_tostring(L, -1);
        }

        lua_getfield(L, 1, "interface");
        if ( lua_isstring(L, -1) )
        {
            interface = lua_tostring(L, -1);
        }

        lua_getfield(L, 1, "method");
        if ( !lua_isstring(L, -1) )
        {
            luaL_error(L, "expecting 'method' field with string value");
        }
        else
        {
            method = lua_tostring(L, -1);
        }
    }
    else
    {
        luaL_checkany(L, lua_absindex(L,-4));
        path = luaL_checkstring(L, lua_absindex(L,-3));
        luaL_checkany(L,lua_absindex(L,-2));
        method = luaL_checkstring(L, lua_absindex(L,-1));

        if ( lua_isstring(L, -4) )
        {
            destination = lua_tostring(L, -4);
        }

        if ( lua_isstring(L, -2) )
        {
            interface = lua_tostring(L, -2);
        }
    }


    dbusMsg = dbus_message_new_method_call(destination, path, interface, method);
    if ( NULL == dbusMsg )
    {
        luaL_error(L, "failed to allocate D-Bus method call message");
    }

    msgUd = (l2dbus_Message*)l2dbus_objectNew(L, sizeof(*msgUd),
                                                 L2DBUS_MESSAGE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Message userdata=%p", msgUd));

    if ( NULL == msgUd )
    {
        dbus_message_unref(dbusMsg);
        luaL_error(L, "failed to allocate userdata for DBus message");
    }
    else
    {
        msgUd->msg = dbusMsg;
    }

    return 1;
}


/**
 @function newMethodReturn

 Creates a new D-Bus method return Message.

 Constructs a D-Bus method return message based on a method call message.

 @tparam userdata callMsg The D-Bus call message on which to
 form the D-Bus return message.
 @treturn userdata Message userdata object for a return message.
 */
static int
l2dbus_newMessageMethodReturn
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;
    DBusMessage* replyMsg;
    l2dbus_Message* replyUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, -1, L2DBUS_MESSAGE_MTBL_NAME);
    luaL_argcheck(L, DBUS_MESSAGE_TYPE_METHOD_CALL == dbus_message_get_type(msgUd->msg), 1,
                 "must be a D-Bus method call message");

    replyMsg = dbus_message_new_method_return(msgUd->msg);
    if ( NULL == replyMsg )
    {
        luaL_error(L, "failed to allocate D-Bus method return message");
    }

    replyUd = (l2dbus_Message*)l2dbus_objectNew(L, sizeof(*replyUd),
                                                 L2DBUS_MESSAGE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Message userdata=%p", msgUd));

    if ( NULL == msgUd )
    {
        dbus_message_unref(replyMsg);
        luaL_error(L, "failed to allocate userdata for DBus message");
    }
    else
    {
        replyUd->msg = replyMsg;
    }

    return 1;
}


/**
 A D-Bus signal message "initializer" table.

 This table can be used in lieu of a parameter list to initialize a new
 D-Bus signal message.

 @see newSignal

 @table SignalInitializer
 @field name (string) [Req] The name of the signal.
 @field interface (string) [Req] The interface the signal is emitted from.
 @field path (string) [Req] The path to the object emitting the signal.
 */


/**
 @function newSignal

 Creates a new D-Bus signal message.

 Constructs a D-Bus signal message either using an initializer @{SignalInitializer|table}
 or parameter list.

 @tparam string|table name The signal name **or** a
 @{SignalInitializer} table containing all the signal parameters. If
 a table is specified then the parameters following the table are ignored.
 @tparam ?string|nil interface The interface the signal is emitted from. Ignored
 if the first parameter is a table.
 @tparam string path The path to the object emitting the signal. Ignored
 if the first parameter is a table.
 @treturn userdata Message userdata object for a signal.
 */
static int
l2dbus_newMessageSignal
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;
    DBusMessage* dbusMsg;
    const char* path = NULL;
    const char* interface = NULL;
    const char* name = NULL;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    if ( LUA_TTABLE == lua_type(L, 1) )
    {
        luaL_checkstack(L, 3, "cannot grow Lua stack to parse arguments");
        lua_getfield(L, 1, "path");
        if ( !lua_isstring(L, -1) )
        {
            luaL_error(L, "expecting 'path' field with string value");
        }
        else
        {
            path = lua_tostring(L, -1);
        }
        lua_getfield(L, 1, "interface");
        if ( !lua_isstring(L, -1) )
        {
            luaL_error(L, "expecting 'interface' field with string value");
        }
        else
        {
            interface = lua_tostring(L, -1);
        }
        lua_getfield(L, 1, "name");
        if ( !lua_isstring(L, -1) )
        {
            luaL_error(L, "expecting 'name' field with string value");
        }
        else
        {
            name = lua_tostring(L, -1);
        }
    }
    else
    {
        path = luaL_checkstring(L, lua_absindex(L,-3));
        interface = luaL_checkstring(L, lua_absindex(L,-2));
        name = luaL_checkstring(L, lua_absindex(L,-1));
    }

    dbusMsg = dbus_message_new_signal(path, interface, name);
    if ( NULL == dbusMsg )
    {
        luaL_error(L, "failed to allocate D-Bus signal message");
    }

    msgUd = (l2dbus_Message*)l2dbus_objectNew(L, sizeof(*msgUd),
                                                 L2DBUS_MESSAGE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Message userdata=%p", msgUd));

    if ( NULL == msgUd )
    {
        dbus_message_unref(dbusMsg);
        luaL_error(L, "failed to allocate userdata for DBus message");
    }
    else
    {
        msgUd->msg = dbusMsg;
    }

    return 1;
}


/**
 @function newError

 Creates a new D-Bus error message based on a request message.

 @tparam userdata callMsg The D-Bus call message on which to
 base the D-Bus error message.
 @tparam string errName The error name for the message. It must be
 a valid name according to the syntax given in the D-Bus
 <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">specification</a>.
 @tparam ?string|nil errMsg The error message or **nil** if
 there is not one.
 @treturn userdata Message userdata object for a error message.
 */
static int
l2dbus_newMessageError
    (
    lua_State*  L
    )
{
    l2dbus_Message* replyMsgUd;
    DBusMessage* errDbusMsg;
    l2dbus_Message* errMsgUd;
    const char* errName;
    const char* errMsg = NULL;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    replyMsgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    luaL_argcheck(L, DBUS_MESSAGE_TYPE_METHOD_CALL == dbus_message_get_type(replyMsgUd->msg), 1,
                 "must be a D-Bus method call message");

    errName = luaL_checkstring(L, 2);
    errMsg = luaL_optstring(L, 3, NULL);

    errDbusMsg = dbus_message_new_error(replyMsgUd->msg, errName, errMsg);
    if ( NULL == errDbusMsg )
    {
        luaL_error(L, "failed to allocate D-Bus error message");
    }

    errMsgUd = (l2dbus_Message*)l2dbus_objectNew(L, sizeof(*errMsgUd),
                                                 L2DBUS_MESSAGE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Message userdata=%p", errMsgUd));

    if ( NULL == errMsgUd )
    {
        dbus_message_unref(errDbusMsg);
        luaL_error(L, "failed to allocate userdata for DBus message");
    }
    else
    {
        errMsgUd->msg = errDbusMsg;
    }

    return 1;
}


/**
 @function copy

 Creates a new D-Bus message based on an existing instance.

 Creates a new message that is an exact replica of the message specified
 except that its serial number is reset to 0 and if the original
 message was "locked" (in the outgoing message queue and thus not
 modifiable) the new message will be unlocked.

 @tparam userdata origMsg The original D-Bus message to copy.
 @treturn userdata Message userdata object for the new copy.
 */
static int
l2dbus_newMessageCopy
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;
    l2dbus_Message* copyUd;
    DBusMessage* msgCopy;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgCopy = dbus_message_copy(msgUd->msg);
    if ( NULL == msgCopy )
    {
        luaL_error(L, "failed to copy D-Bus message");
    }

    copyUd = (l2dbus_Message*)l2dbus_objectNew(L, sizeof(*copyUd),
                                                 L2DBUS_MESSAGE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Message userdata=%p", copyUd));

    if ( NULL == copyUd )
    {
        dbus_message_unref(msgCopy);
        luaL_error(L, "failed to allocate userdata for DBus message");
    }
    else
    {
        copyUd->msg = msgCopy;
    }

    return 1;
}


/**
 @function getType
 @within l2dbus.Message

 Gets the type of the message.

 The valid types include @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL},
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN|MESSAGE_TYPE_METHOD_RETURN},
 @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL}, and
 @{l2dbus.Dbus.MESSAGE_TYPE_ERROR|MESSAGE_TYPE_ERROR}. Other types are
 allowed and all code must silently ignore messages of unknown type. The
 @{l2dbus.Dbus.MESSAGE_TYPE_INVALID|MESSAGE_TYPE_INVALID} type will **never**
 be returned.

 @tparam userdata msg The D-Bus message to get the type.
 @treturn number The D-Bus message type constant.
 */
static int
l2dbus_messageGetType
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    lua_pushinteger(L, dbus_message_get_type(msgUd->msg));

    return 1;
}


/**
 @function setNoReply
 @within l2dbus.Message

 Sets flag indicating the message does not want a reply.

 This method sets a flag in the message that indicates that it does not
 want a reply. If this flag is set the other end of the connection
 *may* (e.g. is not required to) optimize by not sending method return
 or error replies.

 If this flag is set then there is no way to know if the message
 successfully arrived at the remote end. The reply message is used
 to indicate a message has been received. By default a new message
 has this flag set to **false** so the far-end is required to reply.

 @tparam userdata msg The D-Bus message to set the flag.
 @tparam bool flag Set to **true** to indicate not reply is
 necessary. Set to **false** to indicate a reply is needed.
 */
static int
l2dbus_messageSetNoReply
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    luaL_argcheck(L, lua_isboolean(L, 2), 2, "boolean value expected");
    dbus_bool_t noReply = lua_toboolean(L, 2);
    dbus_message_set_no_reply(msgUd->msg, noReply);

    return 0;
}


/**
 @function getNoReply
 @within l2dbus.Message

 Determines if a reply message is needed.

 Returns **true** if the message does not expect a reply.

 @tparam userdata msg The D-Bus message to get the flag status.
 @treturn bool Returns **true** if the message does not expect a
 reply or **false** if a reply is expected.
 */
static int
l2dbus_messageGetNoReply
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    lua_pushboolean(L, dbus_message_get_no_reply(msgUd->msg));
    return 1;
}


/**
 @function setAutoStart
 @within l2dbus.Message

 Sets flag indicating that the owner for the destination name will be
 automatically started before the message is delivered.

 When this flag is set the message is held until a name owner finishes
 starting up or fails. If it fails then the reply will be an error
 message. By default new messages have this set to **true** so that
 auto-starting is the default.

 @tparam userdata msg The D-Bus message on which the flag will be set.
 @tparam bool flag Set to **true** to indicate auto-start is enabled
 or **false** otherwise.
 */
static int
l2dbus_messageSetAutoStart
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    luaL_argcheck(L, lua_isboolean(L, 2), 2, "boolean value expected");
    dbus_bool_t autoStart = lua_toboolean(L, 2);
    dbus_message_set_auto_start(msgUd->msg, autoStart);

    return 0;
}


/**
 @function getAutoStart
 @within l2dbus.Message

 Returns **true** if the provided message will cause the recipient of
 the message (e.g. the destination name) to be auto-started. Otherwise
 **false** is returned and the destination is not auto-started.

 @tparam userdata msg The D-Bus message on which to retrieve the flag.
 @treturn bool Returns **true** to indicate auto-start is enabled
 or **false** otherwise.
 */
static int
l2dbus_messageGetAutoStart
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    lua_pushboolean(L, dbus_message_get_auto_start(msgUd->msg));
    return 1;
}


/**
 @function setPath
 @within l2dbus.Message

 Sets the object path the message is being sent to (for a
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL}) or the
 one the signal is being emitted from (for @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL})

 The path must be a valid D-Bus object path.

 @tparam userdata msg The D-Bus message to set the path.
 @tparam string|nil path The path to set. Can be **nil** to unset the path.
 */
static int
l2dbus_messageSetPath
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;
    const char* path = NULL;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);

    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                  "D-Bus message must be a method call or signal");
    /* Can be either a string or nil */
    luaL_argcheck(L, lua_isnil(L, 2) || lua_isstring(L, 2), 2,
                  "nil or an object path expected");
    if ( lua_isstring(L, 2) )
    {
        path = lua_tostring(L, 2);
    }

    if ( !l2dbus_validatePath(path) )
    {
        luaL_error(L, "invalid D-Bus object path");
    }

    if ( !dbus_message_set_path(msgUd->msg, path) )
    {
        luaL_error(L, "failed to allocate memory for path");
    }
    return 0;
}


/**
 @function getObjectPath
 @within l2dbus.Message

 Gets the object path this message is being sent to (for a
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL}) or the
 path the signal is being emitted from (for @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL})

 The path must be a valid D-Bus object path.

 @tparam userdata msg The D-Bus message to get the path.
 @treturn string|nil The path or **nil** if it's not set.
 */
static int
l2dbus_messageGetPath
    (
    lua_State* L
    )
{
    const char* path = NULL;
    l2dbus_Message* msgUd;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);
    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                  "D-Bus message must be a method call or signal");
    path = dbus_message_get_path(msgUd->msg);

    if ( NULL == path )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, path);
    }
    return 1;
}


/**
 @function hasObjectPath
 @within l2dbus.Message

 Gets the object path this message is being sent to (for a
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL}) or the
 path the signal is being emitted from (for @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL}).

 The path must be a valid D-Bus object path.

 @tparam userdata msg The D-Bus message to get the path.
 @treturn string|nil The path or **nil** if it's not set.
 */
static int
l2dbus_messageHasPath
    (
    lua_State* L
    )
{
    const char* path = NULL;
    l2dbus_Message* msgUd;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);
    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                        "D-Bus message must be a method call or signal");
    path = luaL_checkstring(L, 2);
    lua_pushboolean(L, dbus_message_has_path(msgUd->msg, path));

    return 1;
}


/**
 @function getDecomposedObjectPath
 @within l2dbus.Message

 Gets the decomposed object path this message is being sent to (for a
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL}) or the
 decomposed path the signal is being emitted from (for @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL}).

 The decomposed form is defined as one array element per path component. An
 empty but non-NULL path means the "/" path. So the path "/foo/bar" becomes
 *{"foo", "bar"}* and the path "/" becomes an empty table *{}*.

 @tparam userdata msg The D-Bus message to get the path.
 @treturn table A Lua table/array containing the decomposed path as elements
 of the array.
 */
static int
l2dbus_messageDecomposedPath
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;
    int msgType;
    char** path;
    int idx;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);
    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                        "D-Bus message must be a method call or signal");

    if ( !dbus_message_get_path_decomposed(msgUd->msg, &path) )
    {
        luaL_error(L, "D-Bus failed to allocate memory for decomposed path");
    }

    lua_newtable(L);
    if ( NULL != path )
    {
        for ( idx = 0; NULL != path[idx]; ++idx )
        {
            lua_pushstring(L, path[idx]);
            lua_rawseti(L, -2, idx + 1);
        }

        dbus_free_string_array(path);
    }

    return 1;
}


/**
 @function setInterface
 @within l2dbus.Message

 Sets the interface this message is being sent to (for a
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL}) or the
 interface the signal is being emitted from (for @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL}).

 The interface name must contain only valid characters from the D-Bus
 <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">specification</a>.

 @tparam userdata msg The D-Bus message to set the interface.
 @tparam ?string|nil interface The D-Bus interface name or **nil** if the
 interface is to be unset.
 */
static int
l2dbus_messageSetInterface
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;
    const char* interface = NULL;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);

    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                  "D-Bus message must be a method call or signal");
    /* Can be either a string or nil */
    luaL_argcheck(L, lua_isnil(L, 2) || lua_isstring(L, 2), 2,
                  "nil or an interface string expected");
    if ( lua_isstring(L, 2) )
    {
        interface = lua_tostring(L, 2);
    }

    if ( !l2dbus_validateInterface(interface) )
    {
        luaL_error(L, "invalid D-Bus interface name");
    }

    if ( !dbus_message_set_interface(msgUd->msg, interface) )
    {
        luaL_error(L, "failed to allocate memory for interface");
    }
    return 0;
}


/**
 @function getInterface
 @within l2dbus.Message

 Gets the interface this message is being sent to (for a
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL}) or the
 interface the signal is being emitted from (for @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL}).

 @tparam userdata msg The D-Bus message to get the interface.
 @treturn ?string|nil The D-Bus interface name or **nil** if unset.
 */
static int
l2dbus_messageGetInterface
    (
    lua_State* L
    )
{
    const char* interface = NULL;
    l2dbus_Message* msgUd;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);
    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                  "D-Bus message must be a method call or signal");
    interface = dbus_message_get_interface(msgUd->msg);

    if ( NULL == interface )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, interface);
    }
    return 1;
}


/**
 @function hasInterface
 @within l2dbus.Message

 Checks to see if the message has a matching interface.

 @tparam userdata msg The D-Bus message to compare with the interface.
 @tparam string interface The interface name.
 @treturn bool Returns **true** if the interface field in the message
 header matches, **false** otherwise.
 */
static int
l2dbus_messageHasInterface
    (
    lua_State* L
    )
{
    const char* interface = NULL;
    l2dbus_Message* msgUd;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);
    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                        "D-Bus message must be a method call or signal");
    interface = luaL_checkstring(L, 2);
    lua_pushboolean(L, dbus_message_has_interface(msgUd->msg, interface));

    return 1;
}


/**
 @function setMember
 @within l2dbus.Message

 Sets the interface member this being invoked (for a
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL}) or the
 member name for the signal being emitted (for @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL}).

 The member name must contain only valid characters from the D-Bus
 <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">specification</a>.

 @tparam userdata msg The D-Bus message to set the member name.
 @tparam ?string|nil name The D-Bus member name or **nil** if the
 name is to be unset.
 */
static int
l2dbus_messageSetMember
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;
    const char* member = NULL;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);

    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                  "D-Bus message must be a method call or signal");
    /* Can be either a string or nil */
    luaL_argcheck(L, lua_isnil(L, 2) || lua_isstring(L, 2), 2,
                  "nil or a member string expected");
    if ( lua_isstring(L, 2) )
    {
        member = lua_tostring(L, 2);
    }

    if ( !l2dbus_validateMember(member) )
    {
        luaL_error(L, "invalid D-Bus member name");
    }

    if ( !dbus_message_set_member(msgUd->msg, member) )
    {
        luaL_error(L, "failed to allocate memory for member");
    }
    return 0;
}


/**
 @function getMember
 @within l2dbus.Message

 Gets the interface member being invoked (for a
 @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL}) or emitted
 (for @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL}).

 @tparam userdata msg The D-Bus message to get the member name.
 @treturn ?string|nil The D-Bus member name or **nil** if unset.
 */
static int
l2dbus_messageGetMember
    (
    lua_State* L
    )
{
    const char* member = NULL;
    l2dbus_Message* msgUd;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);
    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                  "D-Bus message must be a method call or signal");
    member = dbus_message_get_member(msgUd->msg);

    if ( NULL == member )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, member);
    }
    return 1;
}


/**
 @function hasMember
 @within l2dbus.Message

 Checks to see if the message has a matching interface member.

 @tparam userdata msg The D-Bus message to compare with the interface member.
 @tparam string interface The member name to compare.
 @treturn bool Returns **true** if the member field in the message
 header matches, **false** otherwise.
 */
static int
l2dbus_messageHasMember
    (
    lua_State* L
    )
{
    const char* member = NULL;
    l2dbus_Message* msgUd;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);
    luaL_argcheck(L, (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
                        (DBUS_MESSAGE_TYPE_SIGNAL == msgType), 1,
                        "D-Bus message must be a method call or signal");
    member = luaL_checkstring(L, 2);
    lua_pushboolean(L, dbus_message_has_member(msgUd->msg, member));

    return 1;
}


/**
 @function setErrorName
 @within l2dbus.Message

 Sets the name of the error message.

 The error name must be fully qualified according the the D-Bus
 <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">specification</a>.

 @tparam userdata msg The D-Bus error message to set the name.
 @tparam ?string|nil name The error name or **nil** to unset this field.
 */
static int
l2dbus_messageSetErrorName
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;
    const char* errorName = NULL;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);

    luaL_argcheck(L, DBUS_MESSAGE_TYPE_ERROR == msgType, 1,
                  "must be a D-Bus error message");
    /* Can be either a string or nil */
    luaL_argcheck(L, lua_isnil(L, 2) || lua_isstring(L, 2), 2,
                  "nil or a error name expected");
    if ( lua_isstring(L, 2) )
    {
        errorName = lua_tostring(L, 2);
    }

    if ( !l2dbus_validateErrorName(errorName) )
    {
        luaL_error(L, "invalid D-Bus error name");
    }

    if ( !dbus_message_set_error_name(msgUd->msg, errorName) )
    {
        luaL_error(L, "failed to allocate memory for error name");
    }
    return 0;
}


/**
 @function getErrorName
 @within l2dbus.Message

 Gets the error name for messages of type
 @{l2dbus.Dbus.MESSAGE_TYPE_ERROR|MESSAGE_TYPE_ERROR}).

 @tparam userdata msg The D-Bus error message to get the error name.
 @treturn ?string|nil The D-Bus error name or **nil** if unset.
 */
static int
l2dbus_messageGetErrorName
    (
    lua_State* L
    )
{
    const char* errorName = NULL;
    l2dbus_Message* msgUd;
    int msgType;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    msgType = dbus_message_get_type(msgUd->msg);
    luaL_argcheck(L, DBUS_MESSAGE_TYPE_ERROR == msgType, 1,
                  "must be a D-Bus error message");
    errorName = dbus_message_get_error_name(msgUd->msg);

    if ( NULL == errorName )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, errorName);
    }
    return 1;
}


/**
 @function setDestination
 @within l2dbus.Message

 Sets the message's destination.

 The destination is the name of another connection on the bus and
 may specify either the *well-known* name or the *unique* name assigned
 by the bus. The destination name must be fully qualified according the the D-Bus
 <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">specification</a>.

 @tparam userdata msg The D-Bus message to set the destination.
 @tparam ?string|nil name The destination or **nil** to unset this field.
 */
static int
l2dbus_messageSetDestination
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;
    const char* destination = NULL;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);

    /* Can be either a string or nil */
    luaL_argcheck(L, lua_isnil(L, 2) || lua_isstring(L, 2), 2,
                  "nil or a destination expected");
    if ( lua_isstring(L, 2) )
    {
        destination = lua_tostring(L, 2);
    }

    if ( !l2dbus_validateBusName(destination) )
    {
        luaL_error(L, "invalid D-Bus destination");
    }

    if ( !dbus_message_set_destination(msgUd->msg, destination) )
    {
        luaL_error(L, "failed to allocate memory for the destination");
    }
    return 0;
}


/**
 @function getDestination
 @within l2dbus.Message

 Gets the destination of a message.

 @tparam userdata msg The D-Bus message to get the destination.
 @treturn ?string|nil The D-Bus destination or **nil** if unset.
 */
static int
l2dbus_messageGetDestination
    (
    lua_State* L
    )
{
    const char* destination = NULL;
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    destination = dbus_message_get_destination(msgUd->msg);

    if ( NULL == destination )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, destination);
    }
    return 1;
}


/**
 @function hasDestination
 @within l2dbus.Message

 Checks to see if the message has a matching destination.

 If the message has no destination specified or has a different destination
 then **false** is returned, otherwise **true**.

 @tparam userdata msg The D-Bus message to compare with the destination.
 @tparam string destination The destination to compare.
 @treturn bool Returns **true** if the destination in the message
 header matches, **false** otherwise.
 */
static int
l2dbus_messageHasDestination
    (
    lua_State* L
    )
{
    const char* destination = NULL;
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    destination = luaL_checkstring(L, 2);
    lua_pushboolean(L, dbus_message_has_destination(msgUd->msg, destination));

    return 1;
}


/**
 @function setSender
 @within l2dbus.Message

 Sets the message sender.

 The sender name must be fully qualified according the the D-Bus
 <a href="http://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names">specification</a>.

 **Note:** You usually don't want to call this method unless you're implementing
 a message bus daemon since that is typically one function of the daemon.

 @tparam userdata msg The D-Bus message to set sender.
 @tparam ?string|nil name The sender or **nil** to unset this field.
 */
static int
l2dbus_messageSetSender
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;
    const char* sender = NULL;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);

    /* Can be either a string or nil */
    luaL_argcheck(L, lua_isnil(L, 2) || lua_isstring(L, 2), 2,
                  "nil or a sender expected");
    if ( lua_isstring(L, 2) )
    {
        sender = lua_tostring(L, 2);
    }

    if ( !dbus_message_set_sender(msgUd->msg, sender) )
    {
        luaL_error(L, "failed to allocate memory for the sender");
    }
    return 0;
}


/**
 @function getSender
 @within l2dbus.Message

 Gets the unique name of the connection which originated this
 message or **nil** if unknown or inapplicable.

 The sender is typically filled in by the message bus. The sender will
 always be the *unique* bus name and **not** one of the (potentially
 multiple) *well-known* names a connection may own.

 @tparam userdata msg The D-Bus message to get the sender.
 @treturn ?string|nil The unique D-Bus sender or **nil** if unset.
 */
static int
l2dbus_messageGetSender
    (
    lua_State* L
    )
{
    const char* sender = NULL;
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    sender = dbus_message_get_sender(msgUd->msg);

    if ( NULL == sender )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, sender);
    }
    return 1;
}


/**
 @function hasSender
 @within l2dbus.Message

 Checks to see if the message has the given unique name as its sender.

 If the message has no sender specified or has a different sender then
 **false** is returned. Messages from the message bus itself will have
 @{l2dbus.Dbus.SERVICE_DBUS|SERVICE_DBUS} as the sender.

 **Note:** A peer application will always have the unique name of the
 connection as the sender and **not** the a sender's *well-known* name.

 @tparam userdata msg The D-Bus message to compare with the sender name.
 @tparam string sender The sender name to compare.
 @treturn bool Returns **true** if the sender in the message
 header matches, **false** otherwise.
 */
static int
l2dbus_messageHasSender
    (
    lua_State* L
    )
{
    const char* sender = NULL;
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    sender = luaL_checkstring(L, 2);
    lua_pushboolean(L, dbus_message_has_sender(msgUd->msg, sender));

    return 1;
}


/**
 @function getSignature
 @within l2dbus.Message

 Gets the type signature of the message.

 The type signature are the arguments in the message payload. The payload
 only includes the *in* arguments for @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL}
 messages and only *out* arguments for @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN|MESSAGE_TYPE_METHOD_RETURN}
 messages. Therefore the result is **not** the complete signature of the entire C++ style method.

 @tparam userdata msg The D-Bus message to get the signature.
 @treturn ?string|nil The D-Bus signature in terms of type codes or **nil** if unset.
 */
static int
l2dbus_messageGetSignature
    (
    lua_State* L
    )
{
    const char* signature = NULL;
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    signature = dbus_message_get_signature(msgUd->msg);

    if ( NULL == signature )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, signature);
    }
    return 1;
}


/**
 @function hasSignature
 @within l2dbus.Message

 Checks to see if the message has the given signature.

 @tparam userdata msg The D-Bus message to compare with the signature.
 @tparam string signature The signature to compare.
 @treturn bool Returns **true** if the signature matches, **false** otherwise.
 */
static int
l2dbus_messageHasSignature
    (
    lua_State* L
    )
{
    const char* signature = NULL;
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    signature = luaL_checkstring(L, 2);
    lua_pushboolean(L, dbus_message_has_signature(msgUd->msg, signature));

    return 1;
}


/**
 @function containsUnixFds
 @within l2dbus.Message

 Checks to see if the message contains Unix file descriptors (fds).

 @tparam userdata msg The D-Bus message to check for Unix file descriptors.
 @treturn bool Returns **true** if the message contains Unix file descriptors,
 **false** otherwise.
 */
static int
l2dbus_messageContainsUnixFds
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    lua_pushboolean(L, dbus_message_contains_unix_fds(msgUd->msg));

    return 1;
}


/**
 @function setSerial
 @within l2dbus.Message

 Sets the message serial number.

 The serial number can only be done **once** for a message.

 **Note:** The underlying @{l2dbus.Connection|Connection} will automatically
 set the serial to an appropriate value when it is enqueued to be sent. This
 function typically only needs to be used in situations where the message is
 being encapsulated in another protocol or it's being sent via a mechanism
 outside of the standard @{l2dbus.Connection|Connection} methods.

 @tparam userdata msg The D-Bus message to set the serial number.
 @tparam number serialNum The integral serial number to set.
 */
static int
l2dbus_messageSetSerial
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;
    dbus_uint32_t serial;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    serial = luaL_checkinteger(L, 2);

    dbus_message_set_serial(msgUd->msg, serial);
    return 0;
}


/**
 @function getSerial
 @within l2dbus.Message

 Returns the serial number of the message or zero (0) if none has been set.

 The message's serial number is provided by the application sending the
 message and is used to identify replies to the message. All messages received
 on a connection will have a serial provided by the remote application.

 @tparam userdata msg The D-Bus message to get the serial number.
 @treturn number The serial number of the message.
 */
static int
l2dbus_messageGetSerial
    (
    lua_State* L
    )
{
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    lua_pushinteger(L, dbus_message_get_serial(msgUd->msg));

    return 1;
}


/**
 @function msgTypeToString

 Converts the D-Bus message type into a machine-readable string.

 <ul>
 <li>@{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL|MESSAGE_TYPE_METHOD_CALL} -> "method_call"</li>
 <li>@{l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN|MESSAGE_TYPE_METHOD_RETURN} -> "method_return"</li>
 <li>@{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL|MESSAGE_TYPE_SIGNAL} -> "signal"</li>
 <li>@{l2dbus.Dbus.MESSAGE_TYPE_ERROR|MESSAGE_TYPE_ERROR} -> "error"</li>
 <li>@{l2dbus.Dbus.MESSAGE_TYPE_INVALID|MESSAGE_TYPE_INVALID} -> "invalid"</li>
 </ul>

 @tparam number msgType The D-Bus message type.
 @treturn string The machine-readable message type string.
 */
static int
l2dbus_messageTypeToString
    (
    lua_State* L
    )
{
    const char* str = NULL;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    str = dbus_message_type_to_string(luaL_checkinteger(L, 1));
    lua_pushstring(L, str);

    return 1;
}


/**
 @function addArgs
 @within l2dbus.Message

 Append Lua arguments to a D-Bus message.

 The Lua arguments are converted to their equivalent D-Bus types
 before encoding them into the message using a set of heuristics.
 A Lua error is generated if any errors are detected during the
 encoding process.

 @tparam userdata msg   D-Bus message to append arguments to.
 @tparam args ... Lua arguments to append to the message.
 */
static int
l2dbus_messageAddArgs
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;
    int nArgs = lua_gettop(L) - 1;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);

    if ( nArgs > 0 )
    {
        l2dbus_transcodeLuaArgsToDbus(L, msgUd->msg, 2, nArgs);
    }

    return 0;
}


/**
 @function addArgsBySignature
 @within l2dbus.Message

 Append Lua arguments to a D-Bus message using signature.

 The Lua arguments are converted to their equivalent D-Bus types
 using the provide D-Bus signature. The signature is used as guide
 so that the conversions are explicit. If an error is encountered
 in the conversion process a Lua error is thrown. The provided
 signature **must** be a valid message signature.

 @tparam userdata msg   D-Bus message to append arguments to.
 @tparam string signature The valid D-Bus signature for the arguments.
 @tparam args ... Lua arguments to append to the message.
 */
static int
l2dbus_messageAddArgsBySignature
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;
    int nArgs = lua_gettop(L) - 2;
    const char* signature;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);

    signature = luaL_checkstring(L, 2);

    if ( nArgs > 0 )
    {
        l2dbus_transcodeLuaArgsToDbusBySignature(L, msgUd->msg, 3,
                                                    nArgs, signature);
    }

    return 0;
}


/**
 @function getArgs
 @within l2dbus.Message

 Retrieve the arguments from the D-Bus message.

 This method unmarshalls the arguments from the D-Bus message
 converting them to equivalent Lua types. The arguments are passed
 back as multiple return values. If there is an error then a Lua
 error is thrown.

 @tparam userdata msg   D-Bus message to extract arguments.
 @treturn ... Lua arguments passed out as multiple return values.
 */
static int
l2dbus_messageGetArgs
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);

    return l2dbus_transcodeDbusArgsToLua(L, msgUd->msg);
}


/**
 @function getArgsAsArray
 @within l2dbus.Message

 Retrieve the arguments from the D-Bus message and return as an array.

 This method unmarshalls the arguments from the D-Bus message
 converting them to equivalent Lua types. The arguments are returned
 in a Lua table treated as an array. The arguments are ordered arg[1..N]
 in the array. If there is an error then a Lua error is thrown.

 @tparam userdata msg   D-Bus message to extract arguments.
 @treturn array Lua arguments returned in an array.
 */
static int
l2dbus_messageGetArgsAsArray
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);

    return l2dbus_transcodeDbusArgsToLuaArray(L, msgUd->msg);
}


/**
 @function marshallToArray
 @within l2dbus.Message

 Marshall the D-Bus message data to a byte array.

 This method marshalls the contents of the D-Bus message into a Lua array
 of numbers (bytes). The array represents a binary representation of the
 D-Bus message.

 @tparam userdata msg D-Bus message to extract binary data.
 @treturn array Array of numbers containing the binary representation of
 the message.
 */
static int
l2dbus_messageMarshallToArray
    (
    lua_State*  L
    )
{
    l2dbus_Message* msgUd;
    char* msgBuf;
    int msgBufLen = 0;
    int idx;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    msgUd = luaL_checkudata(L, 1, L2DBUS_MESSAGE_MTBL_NAME);
    if ( !dbus_message_marshal(msgUd->msg, &msgBuf, &msgBufLen) )
    {
        luaL_error(L, "failed to allocate memory for D-Bus message");
    }
    else
    {
        lua_createtable(L, msgBufLen, 0);
        for ( idx = 0; idx < msgBufLen; ++idx )
        {
            lua_pushinteger(L, msgBuf[idx]);
            lua_rawseti(L, -2, idx);
        }
        dbus_free(msgBuf);
    }

    return 1;
}


/**
 @function unmarshallToMessage

 Unmarshalls a binary array into a D-Bus message.

 This method takes an array of numbers representing the binary
 representation of a D-Bus message and turns it into a new message. This
 is the opposite operation performed by @{marshallToArray}.

 @tparam array msgBuf Array of numbers containing the binary representation of
 the message.
 @return userdata A D-Bus message created from the binary representation.
 */
static int
l2dbus_messageUnmarshallToMessage
    (
    lua_State*  L
    )
{
    size_t  arrayLen;
    size_t  idx;
    char*   buf;
    DBusMessage* dbusMsg = NULL;
    l2dbus_Message* msgUd;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    luaL_argcheck(L, LUA_TTABLE == lua_type(L, 1), 1, "Lua array expected");

    arrayLen = lua_rawlen(L, 1);
    buf = l2dbus_malloc(arrayLen);
    if ( NULL == buf )
    {
        luaL_error(L, "failed to allocate buffer to demarshall message");
    }

    /* Copy the Lua array to a regular C array */
    for ( idx = 1; idx <= arrayLen; ++idx )
    {
        lua_rawgeti(L, 1, idx);
        buf[idx-1] = (unsigned char)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }

    /* Try to demarshall it */
    dbusMsg = dbus_message_demarshal(buf, arrayLen, NULL);
    l2dbus_free(buf);

    if ( NULL == dbusMsg )
    {
        luaL_error(L, "failed to demarshall message");
    }

    msgUd = (l2dbus_Message*)l2dbus_objectNew(L, sizeof(*msgUd),
                                                     L2DBUS_MESSAGE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Message userdata=%p", msgUd));

    if ( NULL == msgUd )
    {
        dbus_message_unref(dbusMsg);
        luaL_error(L, "failed to allocate userdata for DBus message");
    }
    else
    {
        msgUd->msg = dbusMsg;
    }

    return 1;
}


/**
 @function validateSignature

 Check a type signature for validity.

 This function checks the validity of the type signature against the D-Bus
 specification.

 @tparam string signature The signature to validate.
 @treturn bool Returns **true** if the signature is valid, **false** otherwise.
 @treturn ?string|nil An error message if there signature is invalid, **nil** otherwise.
 */
static int
l2dbus_messageValidateSignature
    (
    lua_State*  L
    )
{
    const char* signature;
    DBusError dbusErr;

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);

    signature = luaL_checkstring(L, 1);

    dbus_error_init(&dbusErr);
    if ( !dbus_signature_validate(signature, &dbusErr) )
    {
        lua_pushboolean(L, L2DBUS_FALSE);
        lua_pushfstring(L, "%s", dbusErr.message ? dbusErr.message : "");
    }
    else
    {
        lua_pushboolean(L, L2DBUS_TRUE);
        lua_pushnil(L);
    }

    dbus_error_free(&dbusErr);

    return 2;
}


/**
 * @brief Called by Lua VM to GC/reclaim the D-Bus Message userdata.
 *
 * This method is called by the Lua VM to reclaim the D-Bus Message
 * userdata.
 *
 * @return nil
 *
 */
static int
l2dbus_messageDispose
    (
    lua_State*  L
    )
{
    l2dbus_Message* ud = (l2dbus_Message*)luaL_checkudata(L, -1,
                                        L2DBUS_MESSAGE_MTBL_NAME);

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: message (userdata=%p)", ud));

    if ( ud->msg != NULL )
    {
        L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Unref msg type: %s  serial #: %d",
                dbus_message_type_to_string(dbus_message_get_type(ud->msg)),
                dbus_message_get_serial(ud->msg)));
        dbus_message_unref(ud->msg);
    }

    return 0;
}


/*
 * Define the methods of the D-Bus Message class
 */
static const luaL_Reg l2dbus_messageMetaTable[] = {
    {"getType", l2dbus_messageGetType},
    {"setNoReply", l2dbus_messageSetNoReply},
    {"getNoReply", l2dbus_messageGetNoReply},
    {"setAutoStart", l2dbus_messageSetAutoStart},
    {"getAutoStart", l2dbus_messageGetAutoStart},
    {"setObjectPath", l2dbus_messageSetPath},
    {"getObjectPath", l2dbus_messageGetPath},
    {"hasObjectPath", l2dbus_messageHasPath},
    {"getDecomposedObjectPath", l2dbus_messageDecomposedPath},
    {"setInterface", l2dbus_messageSetInterface},
    {"getInterface", l2dbus_messageGetInterface},
    {"hasInterface", l2dbus_messageHasInterface},
    {"setMember", l2dbus_messageSetMember},
    {"getMember", l2dbus_messageGetMember},
    {"hasMember", l2dbus_messageHasMember},
    {"setErrorName", l2dbus_messageSetErrorName},
    {"getErrorName", l2dbus_messageGetErrorName},
    {"setDestination", l2dbus_messageSetDestination},
    {"getDestination", l2dbus_messageGetDestination},
    {"hasDestination", l2dbus_messageHasDestination},
    {"setSender", l2dbus_messageSetSender},
    {"getSender", l2dbus_messageGetSender},
    {"hasSender", l2dbus_messageHasSender},
    {"getSignature", l2dbus_messageGetSignature},
    {"hasSignature", l2dbus_messageHasSignature},
    {"containsUnixFds", l2dbus_messageContainsUnixFds},
    {"setSerial", l2dbus_messageSetSerial},
    {"getSerial", l2dbus_messageGetSerial},
    {"addArgs", l2dbus_messageAddArgs},
    {"addArgsBySignature", l2dbus_messageAddArgsBySignature},
    {"getArgs", l2dbus_messageGetArgs},
    {"getArgsAsArray", l2dbus_messageGetArgsAsArray},
    {"marshallToArray", l2dbus_messageMarshallToArray},
    {"__gc", l2dbus_messageDispose},
    {NULL, NULL},
};


/**
 * @brief Wraps an underlying D-Bus message is a Lua userdata.
 *
 * This method wraps or embeds a raw D-Bus message into
 * a Lua userdata container. It can optionally add a reference
 * if it's owned by the container.
 *
 * @param [in] L        The Lua state.
 * @param [in] msg      The raw D-Bus message to wrap.
 * @param [in] addRef   Indication of whether to increment the
 * message ref-count (true) or not (false).
 * @return The Lua userdata pointer,
 *
 */
l2dbus_Message*
l2dbus_messageWrap
    (
    lua_State*          L,
    struct DBusMessage* msg,
    l2dbus_Bool         addRef
    )
{
    l2dbus_Message* msgUd = (l2dbus_Message*)l2dbus_objectNew(L, sizeof(*msgUd),
                                                     L2DBUS_MESSAGE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Wrap Message userdata=%p (addRef=%s)",
                msgUd, addRef ? "true" : "false"));
    if ( NULL != msgUd )
    {
        if ( addRef )
        {
            dbus_message_ref(msg);
        }
        msgUd->msg = msg;
    }

    return msgUd;
}


/**
 * @brief Creates the Message sub-module.
 *
 * This function creates a metatable entry for the Message userdata
 * and simulates opening the Message sub-module.
 *
 * @return A table defining the Message sub-module
 *
 */
void
l2dbus_openMessage
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_MESSAGE_TYPE_ID,
            l2dbus_messageMetaTable));
    lua_newtable(L);
    lua_pushcfunction(L, l2dbus_newMessage);
    lua_setfield(L, -2, "new");

    lua_pushcfunction(L, l2dbus_newMessageMethodCall);
    lua_setfield(L, -2, "newMethodCall");

    lua_pushcfunction(L, l2dbus_newMessageMethodReturn);
    lua_setfield(L, -2, "newMethodReturn");

    lua_pushcfunction(L, l2dbus_newMessageSignal);
    lua_setfield(L, -2, "newSignal");

    lua_pushcfunction(L, l2dbus_newMessageError);
    lua_setfield(L, -2, "newError");

    lua_pushcfunction(L, l2dbus_newMessageCopy);
    lua_setfield(L, -2, "copy");

    lua_pushcfunction(L, l2dbus_messageTypeToString);
    lua_setfield(L, -2, "msgTypeToString");

    lua_pushcfunction(L, l2dbus_messageUnmarshallToMessage);
    lua_setfield(L, -2, "unmarshallToMessage");

    lua_pushcfunction(L, l2dbus_messageValidateSignature);
    lua_setfield(L, -2, "validateSignature");

/**
 @messageType INVALID
 This value is never a valid message type.
 Equivalent to @{l2dbus.Dbus.MESSAGE_TYPE_INVALID}.
 */
    lua_pushstring(L, "INVALID");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_INVALID);
    lua_rawset(L, -3);

/**
 @messageType METHOD_CALL
 Message type of a method call.
 Equivalent to @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_CALL}.
 */
    lua_pushstring(L, "METHOD_CALL");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_METHOD_CALL);
    lua_rawset(L, -3);

/**
 @messageType METHOD_RETURN
 Message type of a method return.
 Equivalent to @{l2dbus.Dbus.MESSAGE_TYPE_METHOD_RETURN}.
 */
    lua_pushstring(L, "METHOD_RETURN");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_METHOD_RETURN);
    lua_rawset(L, -3);

/**
 @messageType ERROR
 Message type of an error.
 Equivalent to @{l2dbus.Dbus.MESSAGE_TYPE_ERROR}.
 */
    lua_pushstring(L, "ERROR");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_ERROR);
    lua_rawset(L, -3);

/**
 @messageType SIGNAL
 Message type of a signal.
 Equivalent to @{l2dbus.Dbus.MESSAGE_TYPE_SIGNAL}.
 */
    lua_pushstring(L, "SIGNAL");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_SIGNAL);
    lua_rawset(L, -3);
}



