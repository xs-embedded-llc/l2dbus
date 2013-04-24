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
 * @file           l2dbus_core.h
 * @author         Glenn Schmottlach
 * @brief          Global definitions for the module.
 *===========================================================================
 */

#ifndef L2DBUS_CORE_H_

/* Forward Declarations */
struct lua_State;


#define L2DBUS_XSTR(s)   L2DBUS_STR(s)
#define L2DBUS_STR(s)    #s

#ifndef L2DBUS_MAJOR_VERSION
#define L2DBUS_MAJOR_VERSION 1
#endif

#ifndef L2DBUS_MINOR_VERSION
#define L2DBUS_MINOR_VERSION 0
#endif

#ifndef L2DBUS_RELEASE_VERSION
#define L2DBUS_RELEASE_VERSION 0
#endif

#define L2DBUS_VERSION_STRING \
        L2DBUS_XSTR(L2DBUS_MAJOR_VERSION)"." \
        L2DBUS_XSTR(L2DBUS_MINOR_VERSION)"." \
        L2DBUS_XSTR(L2DBUS_RELEASE_VERSION)

#define L2DBUS_VERSION_NUMBER   ((L2DBUS_MAJOR_VERSION << 16) | \
                                (L2DBUS_MINOR_VERSION << 8) | \
                                (L2DBUS_RELEASE_VERSION))

#define L2DBUS_COPYRIGHT        "(c) Copyright 2013 XS-Embedded LLC"
#define L2DBUS_AUTHOR           "Glenn Schmottlach"

void l2dbus_checkModuleInitialized(struct lua_State* L);
int l2dbus_moduleFinalizerRef(struct lua_State* L);
void l2dbus_moduleFinalizerUnref(struct lua_State* L, int ref);


#define L2DBUS_CORE_H_



#endif /* Guard for L2DBUS_CORE_H_ */
