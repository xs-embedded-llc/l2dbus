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
 * @file           l2dbus_transcode.h
 * @author         Glenn Schmottlach
 * @brief          Definition of D-Bus/Lua type transcoding routines
 *******************************************************************************
 */

#ifndef L2DBUS_TRANSCODE_H_
#define L2DBUS_TRANSCODE_H_
#include "lua.h"


typedef struct l2dbus_DbusValue
{
    int valueRef;
    char* signature;
} l2dbus_DbusValue;

void l2dbus_transcodeLuaArgsToDbusBySignature(lua_State* L, DBusMessage* msg, int argIdx,
                                             int nArgs, const char* signature);
void l2dbus_transcodeLuaArgsToDbus(lua_State* L, DBusMessage* msg, int argIdx, int nArgs);
int l2dbus_transcodeDbusArgsToLuaArray(lua_State* L, DBusMessage* msg);
int l2dbus_transcodeDbusArgsToLua(lua_State* L, DBusMessage* msg);
int l2dbus_openTranscode(lua_State* L);

#endif /* Guard for L2DBUS_TRANSCODE_H_ */
