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
