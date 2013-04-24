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
 * @file           l2dbus_serviceobject.h
 * @author         Glenn Schmottlach
 * @brief          Definition of D-Bus service object
 *===========================================================================
 */

#ifndef L2DBUS_SERVICEOBJECT_H_
#define L2DBUS_SERVICEOBJECT_H_

#include "lua.h"
#include "l2dbus_callback.h"
#include "l2dbus_reflist.h"

/* Forward declarations */
struct cdbus_Object;
struct l2dbus_Interface;

typedef struct l2dbus_ServiceObject
{
    struct cdbus_Object*                obj;
    l2dbus_CallbackCtx                  cbCtx;
    l2dbus_RefList                      interfaces;
} l2dbus_ServiceObject;

void l2dbus_openServiceObject(lua_State* L);

#endif /* Guard for L2DBUS_SERVICEOBJECT_H_ */
