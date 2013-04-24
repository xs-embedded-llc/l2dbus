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
 * @file           l2dbus_callback.h        
 * @author         Glenn Schmottlach
 * @brief          Callback related definitions and types.
 *===========================================================================
 */

#ifndef L2DBUS_CALLBACK_H_
#define L2DBUS_CALLBACK_H_

#include "lua.h"

#define L2DBUS_CALLBACK_NOREF_NEEDED    (0)

typedef struct l2dbus_CallbackCtx
{
    int funcRef;
    int userRef;
} l2dbus_CallbackCtx;

void l2dbus_callbackConfigure(lua_State* L);
void l2dbus_callbackShutdown(lua_State* L);
lua_State* l2dbus_callbackGetThread(void);

void l2dbus_callbackInit(l2dbus_CallbackCtx* ctx);
void l2dbus_callbackDestroy(lua_State* L, l2dbus_CallbackCtx* ctx);

void l2dbus_callbackRef(lua_State* L, int funcIdx, int userIdx, l2dbus_CallbackCtx* ctx);
void l2dbus_callbackUnref(lua_State* L, l2dbus_CallbackCtx* ctx);

#endif /* Guard for L2DBUS_CALLBACK_H_ */
