/*===========================================================================
 *
 * Project         l2dbus
 *
 * Released under the MIT License (MIT)
 * Copyright (c) 2013 XS-Embedded LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
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

