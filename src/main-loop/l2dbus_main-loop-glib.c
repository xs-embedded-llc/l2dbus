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
 * @file           l2dbus_main-loop-glib.c
 * @author         Glenn Schmottlach
 * @brief          The glib based main-loop module for L2DBUS.
 *===========================================================================
 */
#include <stdlib.h>
#include "lua.h"
#include "glib.h"
#include "l2dbus_main-loop.h"
#include "cdbus/cdbus.h"
#include "cdbus/main-loop-glib.h"
#include "l2dbus_compat.h"
#include "l2dbus_types.h"
#include "l2dbus_util.h"


/**
The L2DBUS Glib main loop implementation.

This module provides the main-loop abstraction around the Glib library.

 @module l2dbus-glib
 */

#define L2DBUS_MAIN_LOOP_GLIB_MAJOR_VER       (1)
#define L2DBUS_MAIN_LOOP_GLIB_MINOR_VER       (0)
#define L2DBUS_MAIN_LOOP_GLIB_RELEASE_VER     (0)
#define L2DBUS_MAIN_LOOP_GLIB_COPYRIGHT       "(c) Copyright 2013 XS-Embedded LLC"
#define L2DBUS_MAIN_LOOP_GLIB_AUTHOR          "Glenn Schmottlach"

/*
 * Extension of the base l2dbus_MainLoopUserData type
 */
typedef struct l2dbus_MainLoopGlibUserData
{
    /* Must always be declared first */
    struct cdbus_MainLoop* loop;
} l2dbus_MainLoopGlibUserData;



/**
 Glib main loop module version table.

 A table containing version information for the Glib-based main loop module.

 @table mainLoopGlibVersionInfo
 @field glibMajorLink The major version of the linked Glib library.
 @field glibMinorLink The minor version of the linked Glib library.
 @field glibReleaseLink The release version of the linked Glib library.
 @field glibMajorCompiled The major version of the compiled against Glib library.
 @field glibMinorCompiled The minor version of the compiled against Glib library.
 @field glibReleaseCompiled The release version of the compiled against Glib library.
 @field mainLoopGlibMajor The libev main loop major version.
 @field mainLoopGlibMinor The libev main loop minor version.
 @field mainLoopGlibRelease The libev main loop release version.
 @field copyright The L2DBUS Glib main loop module copyright information.
 @field author The L2DBUS Glib main loop author information.
 */

/**
 @function getVersion

 Returns version information about the L2DBUS Glib module.

 This function returns a table containing useful version
 information related to the module itself and underlying
 libraries.

 @treturn table @{mainLoopGlibVersionInfo}
*/
static int
l2dbus_mainLoopGetVersion
    (
    lua_State*  L
    )
{
    lua_newtable(L);
    lua_pushinteger(L, glib_major_version);
    lua_setfield(L, -2, "glibMajorLink");
    lua_pushinteger(L, glib_minor_version);
    lua_setfield(L, -2, "glibMinorLink");
    lua_pushinteger(L, glib_micro_version);
    lua_setfield(L, -2, "glibReleaseLink");
    lua_pushinteger(L, GLIB_MAJOR_VERSION);
    lua_setfield(L, -2, "glibMajorCompiled");
    lua_pushinteger(L, GLIB_MINOR_VERSION);
    lua_setfield(L, -2, "glibMinorCompiled");
    lua_pushinteger(L, GLIB_MICRO_VERSION);
    lua_setfield(L, -2, "glibReleaseCompiled");

    lua_pushinteger(L, L2DBUS_MAIN_LOOP_GLIB_MAJOR_VER);
    lua_setfield(L, -2, "mainLoopEvMajor");
    lua_pushinteger(L, L2DBUS_MAIN_LOOP_GLIB_MINOR_VER);
    lua_setfield(L, -2, "mainLoopEvMinor");
    lua_pushinteger(L, L2DBUS_MAIN_LOOP_GLIB_RELEASE_VER);
    lua_setfield(L, -2, "mainLoopEvRelease");

    lua_pushliteral(L, L2DBUS_MAIN_LOOP_GLIB_COPYRIGHT);
    lua_setfield(L, -2, "copyright");
    lua_pushliteral(L, L2DBUS_MAIN_LOOP_GLIB_AUTHOR);
    lua_setfield(L, -2, "author");

    return 1;
}


/**
 * @brief Called by Lua VM to GC/reclaim the Main Loop userdata.
 *
 * This method is called by the Lua VM to reclaim the Main Loop
 * userdata.
 *
 * @return nil
 *
 */
static int
l2dbus_mainLoopDispose
    (
    lua_State*  L
    )
{
    l2dbus_MainLoopGlibUserData* ud = (l2dbus_MainLoopGlibUserData*)
        luaL_checkudata(L, -1, L2DBUS_MAIN_LOOP_MTBL_NAME);

    /* Free the  underlying libev main loop */
    CDBUS_MAIN_LOOP_GLIB_UNREF(ud->loop);

    return 0;
}


/**
 @function new

 Creates a new L2DBUS Glib-based main loop.

 Constructs a new main loop using an (optionally) provided lightuserdata item
 referencing a Glib main loop, or if none is provided, internally utilize the
 default Glib main loop to handle events. Any lightuserdata that is passed into
 this constructor that references a Glib main loop is **not** owned by this
 object and will not be de-allocated when it is collected by the Lua garbage
 collector. It is the responsibility of the caller to dispose of the loop
 appropriately at program termination as well as keep a reference to it
 during program operation so it is not prematurely collected by the Lua
 garbage collector.

 See <a href="https://developer.gnome.org/glib/2.36/glib-The-Main-Event-Loop.html">Glib documentation</a> for
 additional information on the underlying main loop library.

 @tparam ?lightuserdata|nil loop Optional *raw* Glib main loop pointer passed
 as a lightuserdata. If nil is specified instead then the default Glib main
 loop will be used. This is generally the recommended approach.
 @treturn userdata MainLoop userdata object
 */
static int
l2dbus_mainLoopNew
    (
    lua_State*  L
    )
{
    GMainLoop* glibLoop = NULL;
    l2dbus_MainLoopGlibUserData* loopUd;

    /* Check to see if a Lua libev loop userdata was passed
     * in for use as the main loop.
     */
    int loopType = lua_type(L, -1);

    if ( LUA_TLIGHTUSERDATA == loopType )
    {
        /* We'll assume (big assumption) that this raw pointer is
         * actually a Glib main loop pointer.
         */
        glibLoop = lua_touserdata(L, -1);
    }
    else if ( LUA_TNIL == loopType )
    {
        /* Use the default loop provided by the Glib main loop binding */
        glibLoop = NULL;
    }
    else
    {
        luaL_argcheck(L, 0, 1, "unexpected main loop type");
    }

    loopUd = (l2dbus_MainLoopGlibUserData*)lua_newuserdata(L, sizeof(*loopUd));
    if ( NULL == loopUd )
    {
        luaL_error(L, "Failed to create main loop userdata!");
    }
    else
    {
        /* Assign the main loop meta-table */
        luaL_getmetatable(L, L2DBUS_MAIN_LOOP_MTBL_NAME);
        lua_setmetatable(L, -2);

        loopUd->loop = CDBUS_MAIN_LOOP_GLIB_NEW(glibLoop, CDBUS_FALSE, NULL);
        if ( NULL == loopUd->loop )
        {
            luaL_error(L, "Failed to allocate Glib main loop!");
        }
    }

    return 1;
}


/* Meta-table for Glib Main Loop type */
static const luaL_Reg l2dbus_mainLoopGlibMetaTable[] =
{
    {"__gc", l2dbus_mainLoopDispose},
    {NULL, NULL},
};


/* Module top-level functions */
static const luaL_Reg l2dbus_mainLoopModuleMetaTable[] =
{
    {"new", l2dbus_mainLoopNew},
    {"getVersion", l2dbus_mainLoopGetVersion},
    {NULL, NULL},
};


int
luaopen_l2dbus_glib
    (
    lua_State* L
    )
{
    luaL_checkversion(L);

    /* Make sure the module was compiled with a libev version compatible to the
     * one that this module is linked to.
     */
    if ( !(glib_major_version == GLIB_MAJOR_VERSION
        && glib_minor_version >= GLIB_MINOR_VERSION) )
    {
        luaL_error(L, "Glib version mismatch: "
            "linked version (%d.%d) incompatible with compiled version (%d.%d)",
            glib_major_version, glib_minor_version, GLIB_MAJOR_VERSION,
            GLIB_MINOR_VERSION);
    }

    /* Create a Main Loop meta-table and pop off the meta-table */
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_MAIN_LOOP_TYPE_ID,
        l2dbus_mainLoopGlibMetaTable));

    luaL_newlib(L, l2dbus_mainLoopModuleMetaTable);

    return 1;
}


