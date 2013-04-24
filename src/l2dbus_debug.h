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
 * @file           l2dbus_debug.h        
 * @author         Glenn Schmottlach
 * @brief          Definition of basic debug routines.
 *===========================================================================
 */

#ifndef L2DBUS_DEBUG_H_
#define L2DBUS_DEBUG_H_

/* Forward Definitions */
struct lua_State;

void l2dbus_dumpItem(struct lua_State* L, int idx, const char* prefix);
void l2dbus_dumpTable(struct lua_State* L, int tableIdx, const char* name);
void l2dbus_dumpUserData(struct lua_State* L, int udIdx, const char* prefix);
void l2dbus_dumpStack(struct lua_State* L);


#ifdef TRACE
    #define L2DBUS_DUMPSTACK(L) l2dbus_dumpStack(L)
#else
    #define L2DBUS_DUMPSTACK(L) do { if ( 0 ) l2dbus_dumpStack(L); } while ( 0 )
#endif  /* TRACE */

#endif /* Guard for L2DBUS_DEBUG_H_ */
