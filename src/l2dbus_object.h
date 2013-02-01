/*******************************************************************************
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
 *******************************************************************************
 *******************************************************************************
 * @file           l2dbus_object.h        
 * @author         Glenn Schmottlach
 * @brief          Definition of a weak object registry.
 *******************************************************************************
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
