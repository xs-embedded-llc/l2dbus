/*******************************************************************************
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
 *******************************************************************************
 *******************************************************************************
 * @file           l2dbus_dbus.c        
 * @author         Glenn Schmottlach
 * @brief          Exports shared D-Bus definitions and enumerations.
 *******************************************************************************
 */
#include <stdlib.h>
#include <assert.h>
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_dbus.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "l2dbus_util.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"

const char L2DBUS_DBUS_MTBL_NAME[] = "l2dbus.dbus";

#define L2DBUS_DBUS_STRING_CONST(V) \
    do { \
    lua_pushliteral(L, #V); \
    lua_pushliteral(L, DBUS_##V); \
    lua_rawset(L, -3); \
    } while (0)

#define L2DBUS_DBUS_INT_CONST(V) \
    do { \
    lua_pushliteral(L, #V); \
    lua_pushinteger(L, DBUS_##V); \
    lua_rawset(L, -3); \
    } while (0)

static int
l2dbus_dbusDispose
    (
    lua_State*  L
    )
{
    return 0;
}


static const luaL_Reg l2dbus_dbusMetaTable[] = {
    {"__gc", l2dbus_dbusDispose},
    {NULL, NULL},
};


int
l2dbus_createDbusMetatable
    (
    lua_State* L
    )
{
    if ( luaL_newmetatable(L, L2DBUS_DBUS_MTBL_NAME) )
    {
        /* Assign the methods to this new metatable */
        luaL_setfuncs(L, l2dbus_dbusMetaTable, 0);

        /* Set it's metatable to point to itself */
        lua_pushvalue(L, -1);
        lua_setfield(L, -2, "__index");
    }

    return 1;
}


void
l2dbus_openDbus
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createDbusMetatable(L));

    /* Create a new table to hold all the constants and defines */
    lua_newtable(L);

    /* Initialize the share constant strings */
    L2DBUS_DBUS_STRING_CONST(SERVICE_DBUS);
    L2DBUS_DBUS_STRING_CONST(PATH_DBUS);
    L2DBUS_DBUS_STRING_CONST(PATH_LOCAL);
    L2DBUS_DBUS_STRING_CONST(INTERFACE_DBUS);
    L2DBUS_DBUS_STRING_CONST(INTERFACE_INTROSPECTABLE);
    L2DBUS_DBUS_STRING_CONST(INTERFACE_PROPERTIES);
    L2DBUS_DBUS_STRING_CONST(INTERFACE_PEER);
    L2DBUS_DBUS_STRING_CONST(INTERFACE_LOCAL);

    /* Initialize the shared constants integral values */
    L2DBUS_DBUS_INT_CONST(NAME_FLAG_ALLOW_REPLACEMENT);
    L2DBUS_DBUS_INT_CONST(NAME_FLAG_REPLACE_EXISTING);
    L2DBUS_DBUS_INT_CONST(NAME_FLAG_DO_NOT_QUEUE);
    L2DBUS_DBUS_INT_CONST(REQUEST_NAME_REPLY_PRIMARY_OWNER);
    L2DBUS_DBUS_INT_CONST(REQUEST_NAME_REPLY_IN_QUEUE);
    L2DBUS_DBUS_INT_CONST(REQUEST_NAME_REPLY_EXISTS);
    L2DBUS_DBUS_INT_CONST(REQUEST_NAME_REPLY_ALREADY_OWNER);
    L2DBUS_DBUS_INT_CONST(RELEASE_NAME_REPLY_RELEASED);
    L2DBUS_DBUS_INT_CONST(RELEASE_NAME_REPLY_NON_EXISTENT);
    L2DBUS_DBUS_INT_CONST(RELEASE_NAME_REPLY_NOT_OWNER);
    L2DBUS_DBUS_INT_CONST(START_REPLY_SUCCESS);
    L2DBUS_DBUS_INT_CONST(START_REPLY_ALREADY_RUNNING);
    L2DBUS_DBUS_INT_CONST(TIMEOUT_USE_DEFAULT);
    L2DBUS_DBUS_INT_CONST(TIMEOUT_INFINITE);

    /* Initialize a few enumerated values */
    L2DBUS_DBUS_INT_CONST(BUS_SESSION);
    L2DBUS_DBUS_INT_CONST(BUS_SYSTEM);
    L2DBUS_DBUS_INT_CONST(BUS_STARTER);

    /* Initialize the basic D-Bus message types */
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_INVALID);
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_METHOD_CALL);
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_METHOD_RETURN);
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_ERROR);
    L2DBUS_DBUS_INT_CONST(MESSAGE_TYPE_SIGNAL);

    /* Initialize the D-Bus handler result codes */
    L2DBUS_DBUS_INT_CONST(HANDLER_RESULT_HANDLED);
    L2DBUS_DBUS_INT_CONST(HANDLER_RESULT_NOT_YET_HANDLED);
    L2DBUS_DBUS_INT_CONST(HANDLER_RESULT_NEED_MEMORY);
}


