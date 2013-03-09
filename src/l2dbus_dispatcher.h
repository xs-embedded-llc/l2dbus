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
 * @file           l2dbus_dispatcher.h
 * @author         Glenn Schmottlach
 * @brief          Definitions for the Lua interface to the Dispatcher.
 *===========================================================================
 */

#ifndef L2DBUS_DISPATCHER_H_
#include "lua.h"

/* Forward declarations */
struct cdbus_Dispatcher;

typedef struct l2dbus_Dispatcher
{
    struct cdbus_Dispatcher* disp;
    int loopRef;
    int finalizerRef;

} l2dbus_Dispatcher;

int l2dbus_newDispatcher(lua_State* L);
void l2dbus_openDispatcher(lua_State* L);


#endif /* Guard for L2DBUS_DISPATCHER_H_ */
