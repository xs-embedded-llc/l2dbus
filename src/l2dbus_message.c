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
 * @file           l2dbus_message.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of D-Bus message object wrapper.
 *******************************************************************************
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


static int
l2dbus_messageSetArgs
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


static int
l2dbus_messageSetArgsBySignature
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


static int
l2dbus_messageDemarshallToMessage
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

    arrayLen = lua_objlen(L, 1);
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
        dbus_message_unref(ud->msg);
    }

    return 0;
}



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
    {"msgTypeToString", l2dbus_messageTypeToString},
    {"setArgs", l2dbus_messageSetArgs},
    {"setArgsBySignature", l2dbus_messageSetArgsBySignature},
    {"getArgs", l2dbus_messageGetArgs},
    {"getArgsAsArray", l2dbus_messageGetArgsAsArray},
    {"marshallToArray", l2dbus_messageMarshallToArray},
    {"__gc", l2dbus_messageDispose},
    {NULL, NULL},
};


l2dbus_Message*
l2dbus_messageWrap
    (
    lua_State*          L,
    struct DBusMessage* msg
    )
{
    l2dbus_Message* msgUd = (l2dbus_Message*)l2dbus_objectNew(L, sizeof(*msgUd),
                                                     L2DBUS_MESSAGE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Message userdata=%p", msgUd));
    if ( NULL != msgUd )
    {
        msgUd->msg = msg;
    }

    return msgUd;
}


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

    lua_pushcfunction(L, l2dbus_messageDemarshallToMessage);
    lua_setfield(L, -2, "demarshallToMessage");

    lua_pushcfunction(L, l2dbus_messageValidateSignature);
    lua_setfield(L, -2, "validateSignature");

    lua_pushstring(L, "INVALID");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_INVALID);
    lua_rawset(L, -3);

    lua_pushstring(L, "METHOD_CALL");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_METHOD_CALL);
    lua_rawset(L, -3);

    lua_pushstring(L, "METHOD_RETURN");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_METHOD_RETURN);
    lua_rawset(L, -3);

    lua_pushstring(L, "ERROR");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_ERROR);
    lua_rawset(L, -3);

    lua_pushstring(L, "SIGNAL");
    lua_pushinteger(L, DBUS_MESSAGE_TYPE_SIGNAL);
    lua_rawset(L, -3);
}



