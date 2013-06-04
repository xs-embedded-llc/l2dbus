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
 * @file           l2dbus_main-loop.h
 * @author         Glenn Schmottlach
 * @brief          Global module definitions for L2DBUS main loops.
 *===========================================================================
 */

#ifndef L2DBUS_MAIN_LOOP_H_
#define L2DBUS_MAIN_LOOP_H_

/* Forward Declarations */
struct lua_State;
struct cdbus_MainLoop;

/*
 * The "base" class for Main Loop user data types. The "loop" field **MUST**
 * always be declared first in any derived concrete type.
 */
typedef struct l2dbus_MainLoopUserData
{
    /* Must always be declared first */
    struct cdbus_MainLoop* loop;

    /* Derived Main Loop types can be defined separately but they
     * must have 'loop' declared first. Additional fields can be added
     * following 'loop'.
     */
} l2dbus_MainLoopUserData;

#endif /* Guard for L2DBUS_MAIN_LOOP_H_ */
