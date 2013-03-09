/*===========================================================================
 * 
 * Project         l2dbus
 * (c) Copyright   2013 XS-Embedded LLC
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
 * @file           l2dbus_uint64.h
 * @author         Glenn Schmottlach
 * @brief          Definition of an uint64 Lua type
 *===========================================================================
 */

#ifndef L2DBUS_UINT64_H_
#define L2DBUS_UINT64_H_

#include <stdint.h>
#include "lua.h"

extern const char L2DBUS_UINT64_MTBL_NAME[];

typedef struct l2dbus_Uint64
{
    uint64_t value;
} l2dbus_Uint64;

int l2dbus_uint64Create(lua_State* L, int idx, int base);
void l2dbus_openUint64(lua_State* L);

#endif /* Guard for L2DBUS_UINT64_H_ */
