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
 * @file           l2dbus_core.c
 * @author         Glenn Schmottlach
 * @brief          The core l2dbus binding source.
 *******************************************************************************
 */
#include <stdlib.h>
#include "lua.h"
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_core.h"
#include "l2dbus_types.h"
#include "l2dbus_object.h"
#include "l2dbus_dispatcher.h"
#include "l2dbus_connection.h"
#include "l2dbus_dbus.h"
#include "l2dbus_transcode.h"
#include "l2dbus_message.h"
#include "l2dbus_watch.h"
#include "l2dbus_timeout.h"
#include "l2dbus_trace.h"
#include "l2dbus_util.h"
#include "l2dbus_callback.h"
#include "l2dbus_int64.h"
#include "l2dbus_uint64.h"
#include "l2dbus_pendingcall.h"
#include "l2dbus_serviceobject.h"
#include "l2dbus_interface.h"
#include "l2dbus_introspection.h"

/* Initially module is not initialized */
static l2dbus_Bool gModuleInitialized = L2DBUS_FALSE;

static void
l2dbus_atexit
    (
    void
    )
{
    cdbus_HResult rc;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Shutting CDBUS at exit"));
    rc = cdbus_shutdown();
    if ( CDBUS_FAILED(rc) )
    {
        /* What else should we do? */
        L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Failed shutting down CDBUS: 0x%X", rc));
    }
}


static int
l2dbus_shutdown
    (
    lua_State*  L
    )
{
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Shutting down l2dbus_core"));

    /* Shutdown/release the Lua specific parts. CDBUS will be
     * shut down in an 'atexit' call since we don't know when
     * the final Lua GC will be done.
     */
    l2dbus_callbackShutdown(L);
    /* Module is no longer initialized */
    gModuleInitialized = L2DBUS_FALSE;

    return 0;
}


static int
l2dbus_getVersion
    (
    lua_State*  L
    )
{
    int dbusMajorVer;
    int dbusMinorVer;
    int dbusMicroVer;

    /* Get the *dynamically linked* version of D-Bus */
    dbus_get_version(&dbusMajorVer, &dbusMinorVer, &dbusMicroVer);

    lua_newtable(L);
    lua_pushinteger(L, dbusMajorVer);
    lua_setfield(L, -2, "dbusMajor");
    lua_pushinteger(L, dbusMinorVer);
    lua_setfield(L, -2, "dbusMinor");
    lua_pushinteger(L, dbusMicroVer);
    lua_setfield(L, -2, "dbusRelease");
    lua_pushinteger(L, atoi(L2DBUS_VERSION_MAJOR));
    lua_setfield(L, -2, "l2dbusMajor");
    lua_pushinteger(L, atoi(L2DBUS_VERSION_MINOR));
    lua_setfield(L, -2, "l2dbusMinor");
    lua_pushinteger(L, atoi(L2DBUS_VERSION_RELEASE));
    lua_setfield(L, -2, "l2dbusRelease");
    lua_pushliteral(L, L2DBUS_VERSION_STRING);
    lua_setfield(L, -2, "l2dbusVerStr");
    lua_pushinteger(L, L2DBUS_VERSION_NUMBER);
    lua_setfield(L, -2, "l2dbusVerNum");
    lua_pushliteral(L, L2DBUS_COPYRIGHT);
    lua_setfield(L, -2, "copyright");
    lua_pushliteral(L, L2DBUS_AUTHOR);
    lua_setfield(L, -2, "author");

    return 1;
}


static int
l2dbus_getLocalMachineId
    (
    lua_State*  L
    )
{
    char* machineId;

    machineId = dbus_get_local_machine_id();
    if ( NULL == machineId )
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, machineId);
        dbus_free(machineId);
    }

    return 1;
}


static const luaL_Reg l2dbus_core[] =
{
    {"getVersion", l2dbus_getVersion},
    {"machineId", l2dbus_getLocalMachineId},
    {"shutdown", l2dbus_shutdown},
    {NULL, NULL},
};


void
l2dbus_checkModuleInitialized
    (
    struct lua_State*   L
    )
{
    if ( !gModuleInitialized )
    {
        luaL_error(L, "l2dbus core module is not initialized!");
    }
}


int
luaopen_l2dbus_core
    (
    lua_State* L
    )
{
    luaL_checkversion(L);

    /* Set the default trace level */
#ifdef DEBUG
    l2dbus_traceSetMask(L2DBUS_TRC_FATAL |
                        L2DBUS_TRC_ERROR |
                        L2DBUS_TRC_WARN |
                        L2DBUS_TRC_INFO |
                        L2DBUS_TRC_TRACE |
                        L2DBUS_TRC_DEBUG
                        );
#else
    l2dbus_traceSetMask(L2DBUS_TRC_FATAL |
                        L2DBUS_TRC_ERROR |
                        L2DBUS_TRC_WARN
                        );
#endif

    cdbus_HResult rc = cdbus_initialize();
    if ( CDBUS_FAILED(rc) )
    {
        l2dbus_cdbusError(L, rc, "CDBUS initialization failure");
    }

    /* Register a function to shut CDBUS down at programe exit or module unload */
    atexit(l2dbus_atexit);

    /* Create an object registry that maps light user data pointers
     * to full user data.
     */
    l2dbus_objectRegistryNew(L);

    /* Configure the callback related routines */
    l2dbus_callbackConfigure(L);

    luaL_newlib(L, l2dbus_core);

    l2dbus_openTrace(L);
    lua_setfield(L, -2, "Trace");

    l2dbus_openDbus(L);
    lua_setfield(L, -2, "Dbus");

    l2dbus_openTranscode(L);
    lua_setfield(L, -2, "DbusTypes");

    l2dbus_openPendingCall(L);
    /* You can't directly create a pending call
     * so there is no need to register a table
     */

    l2dbus_openInt64(L);
    lua_setfield(L, -2, "Int64");

    l2dbus_openUint64(L);
    lua_setfield(L, -2, "Uint64");

    l2dbus_openDispatcher(L);
    lua_setfield(L, -2, "Dispatcher");

    l2dbus_openTimeout(L);
    lua_setfield(L, -2, "Timeout");

    l2dbus_openWatch(L);
    lua_setfield(L, -2, "Watch");

    l2dbus_openMessage(L);
    lua_setfield(L, -2, "Message");;

    l2dbus_openConnectionLib(L);
    lua_setfield(L, -2, "Connection");

    l2dbus_openServiceObject(L);
    lua_setfield(L, -2, "ServiceObject");

    l2dbus_openInterface(L);
    lua_setfield(L, -2, "Interface");


    l2dbus_openIntrospection(L);
    lua_setfield(L, -2, "Introspection");


    /* The module has been successfully initialized */
    gModuleInitialized = L2DBUS_TRUE;

    return 1;
}


