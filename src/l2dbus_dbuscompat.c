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
 * @file           l2dbus_dbuscompat.c
 * @author         Glenn Schmottlach
 * @brief          Lua version compatibility layer implementation.
 *===========================================================================
 */
#include <stdlib.h>
#include "l2dbus_dbuscompat.h"
#include "dbus/dbus.h"

l2dbus_Bool
l2dbus_validatePath
    (
    const char* path
    )
{
#if DBUS_VERSION < 0x010512
    return L2DBUS_TRUE;
#else
    return dbus_validate_path(path, NULL);
#endif
}


l2dbus_Bool
l2dbus_validateInterface
    (
    const char* name
    )
{
#if DBUS_VERSION < 0x010512
    return L2DBUS_TRUE;
#else
    return dbus_validate_interface(name, NULL);
#endif
}


l2dbus_Bool
l2dbus_validateMember
    (
    const char* name
    )
{
#if DBUS_VERSION < 0x010512
    return L2DBUS_TRUE;
#else
    return dbus_validate_member(name, NULL);
#endif
}


l2dbus_Bool
l2dbus_validateErrorName
    (
    const char* name
    )
{
#if DBUS_VERSION < 0x010512
    return L2DBUS_TRUE;
#else
    return dbus_validate_error_name(name, NULL);
#endif
}


l2dbus_Bool
l2dbus_validateBusName
    (
    const char* name
    )
{
#if DBUS_VERSION < 0x010512
    return L2DBUS_TRUE;
#else
    return dbus_validate_bus_name(name, NULL);
#endif
}


l2dbus_Bool
l2dbus_validateUtf8
    (
    const char* text
    )
{
#if DBUS_VERSION < 0x010512
    return L2DBUS_TRUE;
#else
    return dbus_validate_utf8(text, NULL);
#endif
}

