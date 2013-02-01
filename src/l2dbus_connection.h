/*******************************************************************************
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
 *******************************************************************************
 *******************************************************************************
 * @file           l2dbus_connection.h        
 * @author         Glenn Schmottlach
 * @brief          Definitions for the Lua interface to a D-Bus Connection.
 *******************************************************************************
 */

#ifndef L2DBUS_CONNECTION_H_
#define L2DBUS_CONNECTION_H_

#include "lua.h"
#include "queue.h"
#include "l2dbus_match.h"
#include "l2dbus_callback.h"

/* Forward declarations */
struct cdbus_Connection;

typedef struct l2dbus_Connection
{
    struct cdbus_Connection*    conn;
    int                         dispUdRef;
    l2dbus_CallbackCtx          cbCtx;
    l2dbus_Match*               nextMatch;
    LIST_HEAD(l2dbus_MatchHead,
                  l2dbus_Match) matches;
} l2dbus_Connection;

int l2dbus_newConnection(lua_State* L);
void l2dbus_openConnectionLib(lua_State* L);

#endif /* Guard for L2DBUS_CONNECTION_H_ */
