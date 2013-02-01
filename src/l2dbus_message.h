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
 * @file           l2dbus_message.h
 * @author         Glenn Schmottlach
 * @brief          Definition of D-Bus message object wrapper.
 *******************************************************************************
 */

#ifndef L2DBUS_MESSAGE_H_
#define L2DBUS_MESSAGE_H_

#include "lua.h"

/* Forward declarations */
struct DBusMessage;

typedef struct l2dbus_Message
{
    struct DBusMessage* msg;
} l2dbus_Message;

l2dbus_Message* l2dbus_messageWrap(lua_State* L, struct DBusMessage* msg);
void l2dbus_openMessage(lua_State* L);

#endif /* Guard for L2DBUS_MESSAGE_H_ */
