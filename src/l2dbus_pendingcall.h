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
 * @file           l2dbus_pendingcall.h
 * @author         Glenn Schmottlach
 * @brief          Definition of D-Bus "pending call" object
 *******************************************************************************
 */

#ifndef L2DBUS_PENDINGCALL_H_
#define L2DBUS_PENDINGCALL_H_

#include "lua.h"
#include "l2dbus_callback.h"

/* Forward declarations */
struct DBusPendingCall;

typedef struct l2dbus_PendingCall
{
    struct DBusPendingCall* pendingCall;
    int                     connRef;
    l2dbus_CallbackCtx      cbCtx;
} l2dbus_PendingCall;

int l2dbus_newPendingCall(lua_State* L, struct DBusPendingCall* pc,
                            int connIdx);
void l2dbus_openPendingCall(lua_State* L);

#endif /* Guard for L2DBUS_PENDINGCALL_H_ */
