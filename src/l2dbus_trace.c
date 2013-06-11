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
 * @file           l2dbus_trace.c
 * @author         Glenn Schmottlach
 * @brief          Implementation trace routines.
 *===========================================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dbus/dbus.h>
#include "l2dbus_trace.h"
#include "lauxlib.h"

/**
 L2DBUS Trace

 This section describes a L2DBUS Trace capabilities.

 The L2DBUS module can (optionally) be compiled with internal diagnostic
 tracing support. The build flag **TRACE** enables or disables the trace
 output by being defined or undefined (#define/#undef) when the module is
 compiled. If the module has been built with trace support then the functions
 in this module will control the types of trace messages that are emitted to
 stderr.

 The trace flags associated with this module are not strictly *levels* of
 trace but rather discrete channels. This means that @{ERROR} traces can
 be turned *on* while @{INFO}, @{WARN} or @{DEBUG} might be suppressed if not
 explicitly enabled (or turned *on*}. Treat these *levels* as individual flags
 that can be selectively enabled or disabled.

 @namespace l2dbus.Trace
 */


static volatile unsigned gsTraceMask = L2DBUS_TRC_ALL;


/**
 * @brief Determines whether the trace is enabled for the given level.
 *
 * @param [in] level    The trace mask or level to compare.
 * @return Returns zero if the trace is **not** enabled, non-zero otherwise.
 */
int
l2dbus_traceIsEnabled
    (
    unsigned    level,
    ...
    )
{
    return (level & gsTraceMask) != 0;
}


/**
 * @brief Optionally prints a prefix in front of a trace message.
 *
 * @param [in] isEnabled    Controls whether or not the prefix is printed.
 * @param [in] file         A (possibly NULL) string referencing a file name.
 * @param [in] funcName     A (possibly NULL) string referencing a function name.
 * @param [in] line         The line number in the file.
 */
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


/**
 * @brief Optionally prints a trace message depending on the trace "level".
 *
 * @param [in] level    Trace mask controlling whether trace appears.
 * @param [in] fmt      A printf-style format string.
 * @param [in] ...      Variable argument list applied against format string.
 */
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


/**
 * @brief Sets the trace mask to enable/disable trace levels.
 *
 * @param [in] mask    A bitmask of trace levels.
 */
void
l2dbus_traceSetMask
    (
    unsigned    mask
    )
{
    gsTraceMask = mask;
}


/**
 * @brief Gets the current trace mask.
 *
 * @return The trace mask.
 */
unsigned
l2dbus_traceGetMask()
{
    return gsTraceMask;
}


/**
 @function setFlags

 Sets the trace flags for the module.

 Multiple trace flags can be turned on at once, e.g.

     l2dbus.Trace.setFlags(l2dbus.Trace.WARN, l2dbus.Trace.ERROR)

 @tparam ... args A list of parameters to turn *on*.
 */
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


/**
 @function getFlags

 Retrieves the trace flags that are currently enabled (*on*).

 A Lua table is returned with the following format:

     {
         mask = 6,      -- A bitmask of all the enabled flags OR'ed together
         flags = {2, 4} -- An array of l2dbus.Trace.XXXX trace/flag constants
     }

 If there are no flags set then the **flags** field references an empty Lua
 table/array.

 @treturn table flags A table contains information of what flags are enabled.
 */
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


/**
 * @brief Optionally prints a trace of a D-Bus message depending on the "level".
 *
 * @param [in] level    Trace mask controlling whether trace appears.
 * @param [in] msg      The D-Bus message to trace.
 */
void
l2dbus_traceMessage
    (
    unsigned            level,
    struct DBusMessage* msg
    )
{
    const cdbus_Char* msgTypeStr ="UNKNOWN";
    cdbus_Int32 msgType = DBUS_MESSAGE_TYPE_INVALID;
    const cdbus_Char* path = NULL;
    const cdbus_Char* intf = NULL;
    const cdbus_Char* name = NULL;
    const cdbus_Char* dest = NULL;
    const cdbus_Char* errName = NULL;


    if ( NULL != msg )
    {
        msgType = dbus_message_get_type(msg);
        msgTypeStr = dbus_message_type_to_string(msgType);
        if ( (DBUS_MESSAGE_TYPE_METHOD_CALL == msgType) ||
            (DBUS_MESSAGE_TYPE_SIGNAL == msgType) )
        {
            path = dbus_message_get_path(msg);
            intf = dbus_message_get_interface(msg);
            name = dbus_message_get_member(msg);
            l2dbus_trace(level, "(Ser=%u) [%s] <%s> %s%s%s",
                dbus_message_get_serial(msg),
                msgTypeStr,
                path ? path : "",
                intf ? intf : "",
                intf ? "." : "",
                name ? name : "");
        }
        else if (DBUS_MESSAGE_TYPE_METHOD_RETURN == msgType)
        {
            dest = dbus_message_get_destination(msg);
            l2dbus_trace(level, "(RSer=%u) [%s] -> %s",
                        dbus_message_get_reply_serial(msg),
                        msgTypeStr,
                        dest ? dest : "");
        }
        else if (DBUS_MESSAGE_TYPE_ERROR == msgType )
        {
            errName = dbus_message_get_error_name(msg);
            l2dbus_trace(level, "(RSer=%u) [%s] %s",
                                dbus_message_get_reply_serial(msg),
                                msgTypeStr,
                                errName ? errName : "");
        }
        else
        {
            l2dbus_trace(level, "(Ser=%u) [%s]",
                                dbus_message_get_serial(msg),
                                msgTypeStr);
        }
    }
}


/**
 * @brief Creates the Trace sub-module.
 *
 * This function simulates opening the Trace sub-module.
 *
 * @return A table defining the Trace sub-module.
 */
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

/**
 @constant OFF
 This flag disables all tracing.
 */
    lua_pushstring(L, "OFF");
    lua_pushinteger(L, L2DBUS_TRC_OFF);
    lua_rawset(L, -3);

/**
 @constant FATAL
 This flag controls the output of *FATAL* category trace messages.
 */
    lua_pushstring(L, "FATAL");
    lua_pushinteger(L, L2DBUS_TRC_FATAL);
    lua_rawset(L, -3);

/**
 @constant ERROR
 This flag controls the output of *ERROR* category trace messages.
 */
    lua_pushstring(L, "ERROR");
    lua_pushinteger(L, L2DBUS_TRC_ERROR);
    lua_rawset(L, -3);

/**
 @constant WARN
 This flag controls the output of *WARN* category trace messages.
 */
    lua_pushstring(L, "WARN");
    lua_pushinteger(L, L2DBUS_TRC_WARN);
    lua_rawset(L, -3);

/**
 @constant INFO
 This flag controls the output of *INFO* category trace messages.
 */
    lua_pushstring(L, "INFO");
    lua_pushinteger(L, L2DBUS_TRC_INFO);
    lua_rawset(L, -3);

/**
 @constant DEBUG
 This flag controls the output of *DEBUG* category trace messages.
 */
    lua_pushstring(L, "DEBUG");
    lua_pushinteger(L, L2DBUS_TRC_DEBUG);
    lua_rawset(L, -3);

/**
 @constant TRACE
 This flag controls the output of *TRACE* category trace messages.
 */
    lua_pushstring(L, "TRACE");
    lua_pushinteger(L, L2DBUS_TRC_TRACE);
    lua_rawset(L, -3);

/**
 @constant ALL
 This flag turns *on* (enables) all the trace flags.
 */
    lua_pushstring(L, "ALL");
    lua_pushinteger(L, L2DBUS_TRC_ALL);
    lua_rawset(L, -3);
}
