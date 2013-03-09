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
 * @file           l2dbus_trace.h
 * @author         Glenn Schmottlach
 * @brief          Definition trace routines.
 *===========================================================================
 */
#ifndef L2DBUS_TRACE_H_
#define L2DBUS_TRACE_H_
#include "cdbus/cdbus.h"

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
    #else
        #define L2DBUS_TRACE(X) \
            do { l2dbus_tracePrintPrefix(l2dbus_traceIsEnabled X, L2DBUS_BASENAME(__FILE__), 0, __LINE__); \
            l2dbus_trace X; } while ( 0 )
    #endif

#else
    #define L2DBUS_TRACE(X) do { if ( 0 ) l2dbus_trace X; } while ( 0 )
#endif


/* Forward Declaration */
struct lua_State;

void l2dbus_openTrace(struct lua_State* L);


#endif /* L2DBUS_TRACE_H_ */
