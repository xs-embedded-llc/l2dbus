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
 * @file           l2dbus_util.h        
 * @author         Glenn Schmottlach
 * @brief          Definition of common utility types, functions, etc...
 *===========================================================================
 */

#ifndef L2DBUS_UTIL_H_
#define L2DBUS_UTIL_H_
#include "cdbus/cdbus.h"
#include "lauxlib.h"
#include "l2dbus_types.h"

/* Forward declaration */
struct ev_loop;

void* l2dbus_isUserData(lua_State* L, int udIdx, const char *typeName);
struct ev_loop* l2dbus_isEvLoop(lua_State* L, int udIdx);
void l2dbus_cdbusError(lua_State* L, cdbus_HResult rc, const char* msg);
int l2dbus_createMetatable(lua_State* L, l2dbus_TypeId typeId, const luaL_Reg* funcs);
l2dbus_Bool l2dbus_isValidIndex(lua_State* L, int idx);
l2dbus_TypeId l2dbus_getMetaTypeId(lua_State* L, int idx);
const char* l2dbus_getTypeName(lua_State* L, int idx);
void l2dbus_getGlobalField(lua_State* L, const char* name);

#endif /* Guard for L2DBUS_UTIL_H_ */
