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
 * @file           l2dbus_dbuscompat.h
 * @author         Glenn Schmottlach
 * @brief          D-Bus version compatibility layer definitions.
 *===========================================================================
 */

#ifndef L2DBUS_DBUSCOMPAT_H_
#define L2DBUS_DBUSCOMPAT_H_
#include "l2dbus_types.h"


l2dbus_Bool l2dbus_validatePath(const char* path);
l2dbus_Bool l2dbus_validateInterface(const char* name);
l2dbus_Bool l2dbus_validateMember(const char* name);
l2dbus_Bool l2dbus_validateErrorName(const char* name);
l2dbus_Bool l2dbus_validateBusName(const char* name);
l2dbus_Bool l2dbus_validateUtf8(const char* text);


#endif /* Guard for L2DBUS_DBUSCOMPAT_H_ */
