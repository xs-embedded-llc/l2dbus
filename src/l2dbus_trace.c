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
 * @file           l2dbus_trace.c
 * @author         Glenn Schmottlach
 * @brief          Implementation trace routines.
 *===========================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdlib.h>
#include "l2dbus_trace.h"
#include "lauxlib.h"

static volatile unsigned gsTraceMask = L2DBUS_TRC_ALL;

int
l2dbus_traceIsEnabled
    (
    unsigned    level,
    ...
    )
{
    return (level & gsTraceMask) != 0;
}


void
l2dbus_tracePrintPrefix
    (
    int         isEnabled,
    const char* file,
    const char* funcName,
    unsigned    line
    )
{
    if ( isEnabled )
    {
        if ( NULL != funcName )
        {
            fprintf(stderr, "%s:%s(%u) ", file, funcName, line);
        }
        else
        {
            fprintf(stderr, "%s(%u) ", file, line);
        }
    }
}


void
l2dbus_trace
    (
    unsigned    level,
    const char* fmt,
    ...
    )
{
    const char* levelStr = "";
    va_list args;

    if ( level & gsTraceMask )
    {
        switch( level )
        {
            case L2DBUS_TRC_OFF:
                break;
            case L2DBUS_TRC_FATAL:
                levelStr = "FATAL";
                break;
            case L2DBUS_TRC_ERROR:
                levelStr = "ERROR";
                break;
            case L2DBUS_TRC_WARN:
                levelStr = "WARN";
                break;
            case L2DBUS_TRC_INFO:
                levelStr = "INFO";
                break;
            case L2DBUS_TRC_DEBUG:
                levelStr = "DEBUG";
                break;
            case L2DBUS_TRC_TRACE:
                levelStr = "TRACE";
                break;
            default:
                break;
        }

        fprintf(stderr, "%s ", levelStr);
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, "\n");
    }
}


void
l2dbus_traceSetMask
    (
    unsigned    mask
    )
{
    gsTraceMask = mask;
}


unsigned
l2dbus_traceGetMask()
{
    return gsTraceMask;
}


static int
l2dbus_traceSetFlags
    (
    lua_State*  L
    )
{
    unsigned mask;
    unsigned l2dbusMask = L2DBUS_TRC_OFF;
    int nArgs = lua_gettop(L);
    int idx;

    for ( idx = 1; idx <= nArgs; ++idx )
    {
        mask = luaL_checkinteger(L, idx);
        switch ( mask )
        {
            case L2DBUS_TRC_OFF:
            case L2DBUS_TRC_FATAL:
            case L2DBUS_TRC_ERROR:
            case L2DBUS_TRC_WARN:
            case L2DBUS_TRC_INFO:
            case L2DBUS_TRC_DEBUG:
            case L2DBUS_TRC_TRACE:
            case L2DBUS_TRC_ALL:
                l2dbusMask |= mask;
                break;

            default:
                luaL_error(L, "unrecognized trace flag value (0x%x)", mask);
                break;
        }
    }

    l2dbus_traceSetMask(l2dbusMask);

    /* Configure the CDBUS library as well */
    cdbus_traceSetMask(l2dbusMask);

    return 0;
}


static int
l2dbus_traceGetFlags
    (
    lua_State*  L
    )
{
    int idx = 1;
    int j;
    static const unsigned flags[] = {L2DBUS_TRC_FATAL, L2DBUS_TRC_ERROR,
                            L2DBUS_TRC_WARN, L2DBUS_TRC_INFO,
                            L2DBUS_TRC_DEBUG, L2DBUS_TRC_TRACE};
    unsigned mask = l2dbus_traceGetMask();
    lua_newtable(L);
    lua_pushinteger(L, mask);
    lua_setfield(L, -2, "mask");

    lua_newtable(L);
    if ( mask == L2DBUS_TRC_OFF )
    {
        lua_pushinteger(L, L2DBUS_TRC_OFF);
        lua_rawseti(L, -2, idx);
        ++idx;
    }
    else
    {
        for ( j = 0; j < sizeof(flags)/sizeof(flags[0]); ++j )
        {
            if ( mask & flags[j] )
            {
                lua_pushinteger(L, flags[j]);
                lua_rawseti(L, -2, idx);
                ++idx;
            }
        }
    }

    /* Assign the array of flags */
    lua_setfield(L, -2, "flags");

    return 1;
}


void
l2dbus_openTrace
    (
    struct lua_State*  L
    )
{

    lua_newtable(L);
    lua_pushcfunction(L, l2dbus_traceSetFlags);
    lua_setfield(L, -2, "setFlags");

    lua_pushcfunction(L, l2dbus_traceGetFlags);
    lua_setfield(L, -2, "getFlags");

    lua_pushstring(L, "OFF");
    lua_pushinteger(L, L2DBUS_TRC_OFF);
    lua_rawset(L, -3);

    lua_pushstring(L, "FATAL");
    lua_pushinteger(L, L2DBUS_TRC_FATAL);
    lua_rawset(L, -3);

    lua_pushstring(L, "ERROR");
    lua_pushinteger(L, L2DBUS_TRC_ERROR);
    lua_rawset(L, -3);

    lua_pushstring(L, "WARN");
    lua_pushinteger(L, L2DBUS_TRC_WARN);
    lua_rawset(L, -3);

    lua_pushstring(L, "INFO");
    lua_pushinteger(L, L2DBUS_TRC_INFO);
    lua_rawset(L, -3);

    lua_pushstring(L, "DEBUG");
    lua_pushinteger(L, L2DBUS_TRC_DEBUG);
    lua_rawset(L, -3);

    lua_pushstring(L, "TRACE");
    lua_pushinteger(L, L2DBUS_TRC_TRACE);
    lua_rawset(L, -3);

    lua_pushstring(L, "ALL");
    lua_pushinteger(L, L2DBUS_TRC_ALL);
    lua_rawset(L, -3);
}
