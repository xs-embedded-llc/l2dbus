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
 * @file           l2dbus_object.h        
 * @author         Glenn Schmottlach
 * @brief          Definition of a weak object registry.
 *===========================================================================
 */

#ifndef L2DBUS_OBJECT_H_
#define L2DBUS_OBJECT_H_
#include "lua.h"
#include "l2dbus_types.h"

void l2dbus_objectRegistryNew(lua_State* L);
int l2dbus_objectRegistryCount(lua_State* L);
void l2dbus_objectRegistryAdd(lua_State* L, void* key, int objIdx);
void* l2dbus_objectRegistryGet(lua_State* L, void* key);
void l2dbus_objectRegistryRemove(lua_State* L, void* key);
void* l2dbus_objectNew(lua_State* L, size_t size, l2dbus_TypeId typeId);


#endif /* Guard for L2DBUS_OBJECT_H_ */
