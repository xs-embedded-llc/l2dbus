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
 * @file           l2dbus_callback.h        
 * @author         Glenn Schmottlach
 * @brief          Callback related definitions and types.
 *******************************************************************************
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

void* l2dbus_callbackAddWeakRef(lua_State* L, int udIdx);
void l2dbus_callbackRemoveWeakRef(lua_State* L, int udIdx);
void* l2dbus_callbackFetchUd(lua_State* L, void* p);

#endif /* Guard for L2DBUS_CALLBACK_H_ */
