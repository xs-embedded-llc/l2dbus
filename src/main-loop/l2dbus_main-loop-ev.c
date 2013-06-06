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
 * @file           l2dbus_main-loop-ev.c
 * @author         Glenn Schmottlach
 * @brief          The libev based main-loop module for L2DBUS.
 *===========================================================================
 */
#include <stdlib.h>
#include "lua.h"
#include "l2dbus_main-loop.h"
#include "cdbus/cdbus.h"
#include "cdbus/main-loop-ev.h"
#include "l2dbus_compat.h"
#include "l2dbus_types.h"
#include "l2dbus_util.h"
#include "l2dbus_module.h"


/**
The L2DBUS libev main loop implementation.

This module provides the main-loop abstraction around the libev library.

 @module l2dbus-ev
 */

#define L2DBUS_MAIN_LOOP_EV_MAJOR_VER       (1)
#define L2DBUS_MAIN_LOOP_EV_MINOR_VER       (0)
#define L2DBUS_MAIN_LOOP_EV_RELEASE_VER     (0)
#define L2DBUS_MAIN_LOOP_EV_COPYRIGHT       "(c) Copyright 2013 XS-Embedded LLC"
#define L2DBUS_MAIN_LOOP_EV_AUTHOR          "Glenn Schmottlach"

/* Lua Libev metatable type name. This would likely break should the
 * implementation of the Lua binding to libev change.
 */
const char* const L2DBUS_LOOP_MT = "ev{loop}";
#define L2DBUS_LIBEV_UNINITIALIZED_DEFAULT_LOOP ((struct ev_loop*)1)

/*
 * Extension of the base l2dbus_MainLoopUserData type
 */
typedef struct l2dbus_MainLoopEvUserData
{
    /* Must always be declared first */
    struct cdbus_MainLoop* loop;

    /*
     * Below are loop specific implementation fields.
     */

    /* Used to reference Lua libev main loop object */
    int loopRef;

    /* Old libev loop user data */
    void* oldLoopUserData;

} l2dbus_MainLoopEvUserData;


/* The Lua thread used to run all libev callbacks */
static lua_State*  gLuaLibevThread = NULL;
static int gLuaLibevThreadRef = LUA_NOREF;


static void
l2dbus_mainLoopThreadInit
    (
    lua_State*   L
    )
{
    if ( NULL == gLuaLibevThread )
    {
        gLuaLibevThread = lua_newthread(L);
        gLuaLibevThreadRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }
}


/**
 @function shutdown

 Shuts down the L2DBUS libev-based main loop module and frees resources.

 This function frees secondary resources acquired when the module is loaded
 and required to support a libev main loop. It does **not** terminate or
 de-allocate the libev-based main loop itself (this should be done prior
 to making this call). After this call is made any main loop created by
 this module should **no longer** be utilized by L2DBUS and doing so may
 result in a system crash.

 Typically, it is unnecessary to shutdown this module since these resources
 will be reclaimed when the client application terminates and the module is
 unloaded. This function is provided to aid in profiling memory usage.
 */
static int
l2dbus_mainLoopThreadFree
    (
    lua_State*  L
    )
{
    luaL_unref(L, LUA_REGISTRYINDEX, gLuaLibevThreadRef);
    gLuaLibevThread = NULL;
    gLuaLibevThreadRef = LUA_NOREF;

    return 0;
}


static struct ev_loop*
l2dbus_isEvLoop
    (
    lua_State*  L,
    int         udIdx
    )
{
    /* Is value a userdata? */
    struct ev_loop* loop = NULL;
    struct ev_loop** p = lua_touserdata(L, udIdx);
    if ( NULL != p )
    {
        /* Does it have a metatable? */
        if ( lua_getmetatable(L, udIdx) )
        {
            lua_getfield(L, LUA_REGISTRYINDEX, L2DBUS_LOOP_MT);
            /* Does it have the correct mt? */
            if ( lua_rawequal(L, -1, -2) )
            {
                loop = *p;
            }
            lua_pop(L, 2);
        }
    }
    return loop;
}


/**
 Libev main loop module version table.

 A table containing version information for the libev-based main loop module.

 @table mainLoopEvVersionInfo
 @field evMajorLink The major version of the linked libev library.
 @field evMinorLink The minor version of the linked libev library.
 @field evMajorCompiled The major version of the compiled against libev library.
 @field evMinorCompiled The minor version of the compiled against libev library.
 @field mainLoopEvMajor The libev main loop major version.
 @field mainLoopEvMinor The libev main loop minor version.
 @field mainLoopEvRelease The libev main loop release version.
 @field copyright The L2DBUS libev main loop module copyright information.
 @field author The L2DBUS libev main loop author information.
 */

/**
 @function getVersion

 Returns version information about the L2DBUS libev module.

 This function returns a table containing useful version
 information related to the module itself and underlying
 libraries.

 @treturn table @{mainLoopEvVersionInfo}
*/
static int
l2dbus_mainLoopGetVersion
    (
    lua_State*  L
    )
{
    lua_newtable(L);
    lua_pushinteger(L, ev_version_major());
    lua_setfield(L, -2, "evMajorLink");
    lua_pushinteger(L, ev_version_minor());
    lua_setfield(L, -2, "evMinorLink");
    lua_pushinteger(L, EV_VERSION_MAJOR);
    lua_setfield(L, -2, "evMajorCompiled");
    lua_pushinteger(L, EV_VERSION_MINOR);
    lua_setfield(L, -2, "evMinorCompiled");

    lua_pushinteger(L, L2DBUS_MAIN_LOOP_EV_MAJOR_VER);
    lua_setfield(L, -2, "mainLoopEvMajor");
    lua_pushinteger(L, L2DBUS_MAIN_LOOP_EV_MINOR_VER);
    lua_setfield(L, -2, "mainLoopEvMinor");
    lua_pushinteger(L, L2DBUS_MAIN_LOOP_EV_RELEASE_VER);
    lua_setfield(L, -2, "mainLoopEvRelease");

    lua_pushliteral(L, L2DBUS_MAIN_LOOP_EV_COPYRIGHT);
    lua_setfield(L, -2, "copyright");
    lua_pushliteral(L, L2DBUS_MAIN_LOOP_EV_AUTHOR);
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
    l2dbus_MainLoopEvUserData* ud = (l2dbus_MainLoopEvUserData*)
        luaL_checkudata(L, -1, L2DBUS_MAIN_LOOP_MTBL_NAME);

    /* Free the  underlying libev main loop */
    CDBUS_MAIN_LOOP_EV_UNREF(ud->loop);

    /* If this was a Lua libev object then unreference it */
    luaL_unref(L, LUA_REGISTRYINDEX, ud->loopRef);

    return 0;
}

static void
l2dbus_mainLoopPreLoop
    (
    cdbus_MainLoop* loop
    )
{
    cdbus_MainLoopEv* mainLoopEv = (cdbus_MainLoopEv*)loop;
    l2dbus_MainLoopEvUserData* loopUd;

    if ( (NULL != mainLoopEv) && (NULL != gLuaLibevThread) )
    {
        loopUd = mainLoopEv->userData;
#if EV_MULTIPLICITY
        loopUd->oldLoopUserData = ev_userdata(mainLoopEv->loop);
        ev_set_userdata(mainLoopEv->loop, gLuaLibevThread);
#else
        loopUd->oldLoopUserData = ev_userdata();
        ev_set_userdata(gLuaLibevThread);
#endif

    }
}


static void
l2dbus_mainLoopPostLoop
    (
    cdbus_MainLoop* loop
    )
{
    cdbus_MainLoopEv* mainLoopEv = (cdbus_MainLoopEv*)loop;
    l2dbus_MainLoopEvUserData* loopUd;

    if ( (NULL != mainLoopEv) && (NULL != gLuaLibevThread) )
    {
        loopUd = mainLoopEv->userData;
#if EV_MULTIPLICITY
        ev_set_userdata(mainLoopEv->loop, loopUd->oldLoopUserData);
#else
        ev_set_userdata(loopUd->oldLoopUserData);
#endif

    }
}


/**
 @function MainLoop.new

 Creates a new L2DBUS libev-based main loop.

 Constructs a new main loop using an (optionally) provided Lua libev
 <a href="https://github.com/brimworks/lua-ev">ev.Loop</a> object,
 a lightuserdata item referencing a libev main loop, or if none is provided,
 internally utilize the default libev main loop to handle events.
 Any Lua libev loop or lightuserdata that is passed into this constructor is
 **not** owned by this object and will not be de-allocated when it is
 collected by the Lua garbage collector. If a Lua ev.Loop userdata is
 passed in it will, however, hold a reference to that. In the case of
 a Lua ev.Loop or lightuserdata object it is the responsibility of the caller
 to dispose of the loop appropriately at program termination.

 *Note:* If passing in a libev main loop (ev.Loop) please insure
 that it has been fully instantiated by either creating it by calling
     loop = ev.Loop.new()
 or calling a Lua libev function that consumes a ev.Loop object
 (e.g. loop:now()). By default the libev ev.Loop objects are lazily realized.
 It's possible to unwittingly pass an unrealized Lua libev main loop to the
 this function which will cause it to fail.

 See <a href="http://software.schmorp.de/pkg/libev.html">libev</a> for
 additional information on the underlying main loop library.

 @tparam ?ev.Loop|lightuserdata|nil loop Optional Lua libev main loop or *raw*
 libev main loop pointer passed as a lightuserdata. If nil is specified instead
 then the default libev main loop will be used. This is generally the
 recommended approach.
 @treturn userdata MainLoop userdata object
 */
static int
l2dbus_mainLoopNew
    (
    lua_State*  L
    )
{
    struct ev_loop* evLoop = NULL;
    l2dbus_MainLoopEvUserData* loopUd;

    /* Check to see if a Lua libev loop userdata was passed
     * in for use as the main loop.
     */
    int loopType = lua_type(L, 1);

    if ( NULL == gLuaLibevThread )
    {
        luaL_error(L, "Module failed to initialized or was shut down");
    }
    else if ( LUA_TUSERDATA == loopType )
    {
        evLoop = l2dbus_isEvLoop(L, 1);
        if ( evLoop == L2DBUS_LIBEV_UNINITIALIZED_DEFAULT_LOOP )
        {
            luaL_error(L, "The Lua libev loop is uninitialized - "
                    "try using ev.Loop.new() to create one");
        }
    }
    else if ( LUA_TLIGHTUSERDATA == loopType )
    {
        /* We'll assume (big assumption) that this raw pointer is
         * actually a libev main loop pointer.
         */
        evLoop = lua_touserdata(L, 1);
    }
    else if ( (LUA_TNONE == loopType) || (LUA_TNIL == loopType) )
    {
        /* Use the default loop provided by the libev main loop binding */
        evLoop = NULL;
    }
    else
    {
        luaL_argcheck(L, 0, 1, "unexpected main loop type");
    }

    loopUd = (l2dbus_MainLoopEvUserData*)lua_newuserdata(L, sizeof(*loopUd));
    if ( NULL == loopUd )
    {
        luaL_error(L, "Failed to create main loop userdata!");
    }
    else
    {
        loopUd->loopRef = LUA_NOREF;
        loopUd->oldLoopUserData = NULL;

        /* Assign the main loop meta-table */
        luaL_getmetatable(L, L2DBUS_MAIN_LOOP_MTBL_NAME);
        lua_setmetatable(L, -2);

        loopUd->loop = CDBUS_MAIN_LOOP_EV_NEW(evLoop, CDBUS_FALSE, loopUd);
        if ( NULL == loopUd->loop )
        {
            luaL_error(L, "Failed to allocate libev main loop!");
        }
        else
        {
            /* Provide a pre/post hooks to set the Lua state used by
             * Lua libev and stored as user data in the evLoop structure.
             */
            loopUd->loop->loopPre = l2dbus_mainLoopPreLoop;
            loopUd->loop->loopPost = l2dbus_mainLoopPostLoop;

            /* If the loop is owned by the Lua libev binding then ... */
            if ( LUA_TUSERDATA == loopType )
            {
                /* Keep a reference to the Lua libev object */
                lua_pushvalue(L, -1);
                loopUd->loopRef = lua_ref(L, LUA_REGISTRYINDEX);
            }
        }
    }

    return 1;
}


/* Meta-table for libev Main Loop type */
static const luaL_Reg l2dbus_mainLoopEvMetaTable[] =
{
    {"__gc", l2dbus_mainLoopDispose},
    {NULL, NULL},
};


/* Module top-level functions */
static const luaL_Reg l2dbus_mainLoopModuleTable[] =
{
    {"getVersion", l2dbus_mainLoopGetVersion},
    {"shutdown", l2dbus_mainLoopThreadFree},
    {NULL, NULL},
};


/* Main loop top-level functions */
static const luaL_Reg l2dbus_mainLoopLoopTable[] =
{
    {"new", l2dbus_mainLoopNew},
    {NULL, NULL},
};

int
luaopen_l2dbus_ev
    (
    lua_State* L
    )
{
    luaL_checkversion(L);

    /* Make sure the module was compiled with a libev version compatible to the
     * one that this module is linked to.
     */
    if ( !(ev_version_major () == EV_VERSION_MAJOR
        && ev_version_minor () >= EV_VERSION_MINOR) )
    {
        luaL_error(L, "Libev version mismatch: "
            "linked version (%d.%d) incompatible with compiled version (%d.%d)",
            ev_version_major(), ev_version_minor(), EV_VERSION_MAJOR,
            EV_VERSION_MINOR);
    }

    /* Create a Lua thread to be used by Lua Libev */
    l2dbus_mainLoopThreadInit(L);

    /* Create a Main Loop meta-table and pop off the meta-table */
    lua_pop(L, l2dbus_createMetatable(L, L2DBUS_MAIN_LOOP_TYPE_ID,
        l2dbus_mainLoopEvMetaTable));

    luaL_newlib(L, l2dbus_mainLoopModuleTable);
    luaL_newlib(L, l2dbus_mainLoopLoopTable);

    /* Assign main loop table to the top-level module table */
    lua_setfield(L, -2, "MainLoop");

    /*
     * ** KLUDGE **
     * Lua has a bug (fixed version 5.2.1) where finalizers may call functions
     * from a dynamic library (module) after the library has been unloaded
     * (see http://www.lua.org/bugs.html#5.2.0-3). This is a very real problem
     * for the main loop modules since it's possible they will be unloaded
     * *before* D-Bus has been fully shut down. This leads to a seg-fault
     * when a program is exits. In order to get around this bug (short of
     * using a Lua version >= 5.2.1) another reference is made to the
     * module's dynamic library so that when Lua does an effective dlclose()
     * on the module it isn't unloaded (e.g. it's ref-count remains greater
     * than 1). As a result the module won't be unloaded until the program
     * exits. It's not a pretty fix but the only solution short of a patch
     * to all earlier versions of Lua.
     */
    l2dbus_moduleRef(L, "l2dbus_ev");

    return 1;
}


