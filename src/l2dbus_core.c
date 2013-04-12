/*===========================================================================
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
 *===========================================================================
 *===========================================================================
 * @file           l2dbus_core.c
 * @author         Glenn Schmottlach
 * @brief          The core l2dbus binding source.
 *===========================================================================
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

/**
The low-level L2DBUS core module.

This module provides access to the low-level functions/methods
that implement the binding to the D-Bus reference library. Loading this core
library introduces the following namespaces into the *l2dbus* module.
Since these are namespaces they are implicitly defined when the *l2dbus* is
required.

The following namespaces are created when the *l2dbus* module is loaded:
</br>
<ul>
<li>l2dbus.Connection</li>
<li>l2dbus.Dbus</li>
<li>l2dbus.DbusTypes</li>
<li>l2dbus.Dispatcher</li>
<li>l2dbus.Int64</li>
<li>l2dbus.Interface</li>
<li>l2dbus.Introspection</li>
<li>l2dbus.Match</li>
<li>l2dbus.Message</li>
<li>l2dbus.PendingCall</li>
<li>l2dbus.ServiceObject</li>
<li>l2dbus.Timeout</li>
<li>l2dbus.Trace</li>
<li>l2dbus.Uint64</li>
<li>l2dbus.Watch</li>
</ul>

 @module l2dbus
 */


/*
 * Define/undefine to enable/disable the call to CDBUS's shutdown
 * function. Generally it is ill-advised to define it because of
 * shutdown sequencing issues between libev and when D-Bus has
 * truly shutdown.
 */
#undef L2DBUS_SHUTDOWN_CDBUS


/**
 * @brief Used to track the lifetime of the module
 *
 * Initially the module is not initialized until it's
 * loaded by Lua. This variable references a global
 * Lua userdata item that should be GC'ed when the
 * module is unloaded or the VM shuts down.
 *
 */
static l2dbus_Bool gModuleFinalizerRef = LUA_NOREF;


/**
 * @brief Shuts down the underlying CDBUS library.
 *
 * An internal call to shutdown the underlying CDBUS library.
 *
 * @return none
 *
 */
static void
l2dbus_shutdownCdbus
    (
    void
    )
{
    /* We actually *cannot* shutdown CDBUS/D-Bus because it takes a while for
     * D-Bus to fully shutdown. If we shut it down then we get a bunch
     * of asserts from libev because it's been shutdown but D-Bus keeps
     * trying to call functions on it via Watches being disabled.
     */
#ifdef L2DBUS_SHUTDOWN_CDBUS
    cdbus_HResult rc;
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Shutting CDBUS at exit"));
    rc = cdbus_shutdown();
    if ( CDBUS_FAILED(rc) )
    {
        /* What else should we do? */
        L2DBUS_TRACE((L2DBUS_TRC_ERROR, "Failed shutting down CDBUS: 0x%X", rc));
    }
#endif
}

/**
 @function shutdown

 Shuts down the L2DBUS module.

 Typically this function should be called prior to
 an L2DBUS application terminating. Once called it
 is no longer safe to call any other L2DBUS
 functions/methods. Strictly speaking, it's not
 necessary to call this function when terminating since
 the Lua VM and OS will implicitly free any underlying
 resources.

 @return nil
 */
static int
l2dbus_shutdown
    (
    lua_State*  L
    )
{
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Shutting down l2dbus_core"));

    l2dbus_moduleFinalizerUnref(L, gModuleFinalizerRef);
    gModuleFinalizerRef = LUA_NOREF;

    return 0;
}


/**
 Version table.

 A table containing version information of the L2DBUS module
 and underlying dependencies.

 @table versionInfo
 @field dbusMajor The D-Bus major version as a number.
 @field dbusMinor The D-Bus minor version as a number.
 @field dbusRelese The D-Bus release version as a number.
 @field l2dbusMajor The L2DBUS major version as a number.
 @field l2dbusMinor The L2DBUS minor version as a number.
 @field l2dbusRelese The L2DBUS release version as a number.
 @field l2dbusVerNum The L2DBUS version as a number.
 @field l2dbusVerStr The L2DBUS version as a string.
 @field cdbusMajor The CDBUS major version as a number.
 @field cdbusMinor The CDBUS minor version as a number.
 @field cdbusRelease The CDBUS release version as a number.
 @field cdbusVerStr The CDBUS version as a string.
 @field copyright The L2DBUS copyright information.
 @field author The L2DBUS author information.
 */

/**
 @function getVersion

 Returns version information about the L2DBUS module.

 This function returns a table containing useful version
 information related to the module itself and underlying
 libraries.

 @treturn table @{versionInfo}
 */
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
    lua_pushinteger(L, CDBUS_MAJOR_VERSION);
    lua_setfield(L, -2, "cdbusMajor");
    lua_pushinteger(L, CDBUS_MINOR_VERSION);
    lua_setfield(L, -2, "cdbusMinor");
    lua_pushinteger(L, CDBUS_RELEASE_VERSION);
    lua_setfield(L, -2, "cdbusRelease");
    lua_pushliteral(L, CDBUS_VERSION_STRING);
    lua_setfield(L, -2, "cdbusVerStr");
    lua_pushinteger(L, L2DBUS_VERSION_NUMBER);
    lua_setfield(L, -2, "l2dbusVerNum");
    lua_pushliteral(L, L2DBUS_COPYRIGHT);
    lua_setfield(L, -2, "copyright");
    lua_pushliteral(L, L2DBUS_AUTHOR);
    lua_setfield(L, -2, "author");

    return 1;
}


/**
 @function machineId

 Returns the D-Bus local machine identifier.

 Obtains the machine UUID of the machine this process is
 running on.

 For more details see:
 <a href="http://dbus.freedesktop.org/doc/api/html/group__DBusMisc.html#ga2b21c9a12fea5f92763441c65ccbfcf9">
 dbus&#95;get&#95;local&#95;machine_id(void)</a>

 @treturn string The local D-Bus machine identifier.
 */
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


/**
 * @brief Adds a reference to the global module finalizer userdata.
 *
 * An internal function used to add a reference to the module's global
 * finalizer userdata. This userdata *should* be the last object in the
 * module GC'ed (finalized) by the Lua VM when it's unloaded or shutdown.
 *
 * @return The index of the reference or LUA_NOREF on failure to
 *         add a reference to the module finalizer userdata.
 *
 */
int
l2dbus_moduleFinalizerRef
    (
    struct lua_State*  L
    )
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, gModuleFinalizerRef);
    if ( LUA_TUSERDATA == lua_type(L, -1) )
    {
        return luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        L2DBUS_TRACE((L2DBUS_TRC_ERROR,
            "Trying to reference the module finalizer after it's been released"));
        return LUA_NOREF;
    }
}


/**
 * @brief Unreferences to the module's global finalizer userdata.
 *
 * An internal function used to add a reference to the module's global
 * finalizer userdata. This userdata *should* be the last object in the module
 * GC'ed (finalized) by the Lua VM when it's unloaded or shutdown.
 *
 * @return The index of the reference or LUA_NOREF on failure to
 *         add a reference to the module finalizer userdata.
 *
 */
void
l2dbus_moduleFinalizerUnref
    (
    struct lua_State*   L,
    int                 ref
    )
{
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
}


/**
 * @brief Called by Lua VM to GC the finalizer table.
 *
 * This method is called by the Lua VM to GC the finalizer
 * userdata. It *should* be the last object destroyed by the
 * Lua VM when this module is unloaded.
 *
 * @return nil
 *
 */
static int
l2dbus_moduleFinalizerDispose
    (
    lua_State*  L
    )
{
    void* p = lua_touserdata(L, -1);
    /* This *should* be the last thing destroyed by the module */
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "GC: module finalizer (userdata=%p)", p));
    l2dbus_callbackShutdown(L);
    l2dbus_shutdownCdbus();

    return 0;
}


static const luaL_Reg l2dbus_coreMetaTable[] =
{
    {"getVersion", l2dbus_getVersion},
    {"machineId", l2dbus_getLocalMachineId},
    {"shutdown", l2dbus_shutdown},
    {NULL, NULL},
};

/* Metatable for the finalizer userdata */
static const luaL_Reg l2dbus_moduleFinalizerMetaTable[] =
{
    {"__gc", l2dbus_moduleFinalizerDispose},
    {NULL, NULL}
};


/**
 * @brief Creates the global module finalizer metatable type.
 *
 * This function creates a metatable entry for the userdata
 * used to track the lifetime of the L2DBUS module.
 *
 * @return nil
 *
 */
static void
l2dbus_openModuleFinalizer
    (
    lua_State*  L
    )
{
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_MODULE_FINALIZER_TYPE_ID,
            l2dbus_moduleFinalizerMetaTable));
}


/**
 * @brief Checks to see if the module was initialized successfully
 *
 * This function checks to see if the module initialized successfully
 * based on whether there is a valid reference to the module's
 * "finalizer" userdata item. It will throw a Lua error and not
 * return if the module is not initialized.
 *
 * @return none
 *
 */
void
l2dbus_checkModuleInitialized
    (
    struct lua_State*   L
    )
{
    if ( gModuleFinalizerRef == LUA_NOREF )
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

    /* Create an userdata type used to shutdown (finalize) the module */
    l2dbus_openModuleFinalizer(L);

    /* Create an object registry that maps light user data pointers
     * to full user data.
     */
    l2dbus_objectRegistryNew(L);

    /* Configure the callback related routines */
    l2dbus_callbackConfigure(L);

    luaL_newlib(L, l2dbus_coreMetaTable);

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
    l2dbus_objectNew(L, 0, L2DBUS_MODULE_FINALIZER_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_INFO, "Created module finalizer instance (userdata=%p)",
                lua_touserdata(L, -1)));
    gModuleFinalizerRef = luaL_ref(L, LUA_REGISTRYINDEX);

    return 1;
}


