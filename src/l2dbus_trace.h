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
 * @file           l2dbus_trace.h
 * @author         Glenn Schmottlach
 * @brief          Definition trace routines.
 *===========================================================================
 */
#ifndef L2DBUS_TRACE_H_
#define L2DBUS_TRACE_H_
#include "cdbus/cdbus.h"

/* Forward Declarations */
struct DBusMessage;

#define L2DBUS_TRC_OFF   CDBUS_TRC_OFF
#define L2DBUS_TRC_FATAL CDBUS_TRC_FATAL
#define L2DBUS_TRC_ERROR CDBUS_TRC_ERROR
#define L2DBUS_TRC_WARN  CDBUS_TRC_WARN
#define L2DBUS_TRC_INFO  CDBUS_TRC_INFO
#define L2DBUS_TRC_DEBUG CDBUS_TRC_DEBUG
#define L2DBUS_TRC_TRACE CDBUS_TRC_TRACE
#define L2DBUS_TRC_ALL   (L2DBUS_TRC_TRACE | L2DBUS_TRC_DEBUG | L2DBUS_TRC_INFO | \
                        L2DBUS_TRC_WARN | L2DBUS_TRC_ERROR | L2DBUS_TRC_FATAL)

void l2dbus_trace(unsigned level, const char* fmt, ...);
int l2dbus_traceIsEnabled(unsigned level, ...);
void l2dbus_tracePrintPrefix(int isEnabled, const char* file, const char* funcName, unsigned line);
void l2dbus_traceSetMask(unsigned mask);
unsigned l2dbus_traceGetMask();
void l2dbus_traceMessage(unsigned level, struct DBusMessage* msg);

/*
 * L2DBUS_TRACE((LVL, FMT, ...))
 *
 * C89 compatible trace routine. Calls to this macro/function should
 * be written as follows:
 *
 *          L2DBUS_TRACE((L2DBUS_TRC_DEBUG, "This value is %d", 1));
 */
#ifdef TRACE
    #include <stdio.h>

    #ifdef linux
        #include <libgen.h>
        #define L2DBUS_BASENAME(X)   basename(X)
    #else
        #define L2DBUS_BASENAME(X)   X
    #endif

    #if (__STDC_VERSION__ >= 199901L)
        #define L2DBUS_TRACE(X) \
            do { l2dbus_tracePrintPrefix(l2dbus_traceIsEnabled X, L2DBUS_BASENAME(__FILE__), __FUNCTION__, __LINE__); \
            l2dbus_trace X; } while ( 0 )
        #define L2DBUS_TRACE_MSG(X) \
            do { l2dbus_tracePrintPrefix(l2dbus_traceIsEnabled X, L2DBUS_BASENAME(__FILE__), __FUNCTION__, __LINE__); \
            l2dbus_traceMessage X; } while ( 0 )
    #else
        #define L2DBUS_TRACE(X) \
            do { l2dbus_tracePrintPrefix(l2dbus_traceIsEnabled X, L2DBUS_BASENAME(__FILE__), 0, __LINE__); \
            l2dbus_trace X; } while ( 0 )
        #define L2DBUS_TRACE_MSG(X) \
            do { l2dbus_tracePrintPrefix(l2dbus_traceIsEnabled X, L2DBUS_BASENAME(__FILE__), 0, __LINE__); \
            l2dbus_traceMessage X; } while ( 0 )
    #endif

#else
    #define L2DBUS_TRACE(X) do { if ( 0 ) l2dbus_trace X; } while ( 0 )
    #define L2DBUS_TRACE_MSG(X) do { if ( 0 ) l2dbus_trace X; } while ( 0 )
#endif


/* Forward Declaration */
struct lua_State;

void l2dbus_openTrace(struct lua_State* L);


#endif /* L2DBUS_TRACE_H_ */
