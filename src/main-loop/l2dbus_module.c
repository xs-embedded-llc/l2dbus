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
 * @file           l2dbus_module.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of the C-module functions.
 *===========================================================================
 */

#include <stdio.h>
#include <string.h>
#include "l2dbus_module.h"
#include "luaconf.h"

#if defined(__linux__) || defined(__APPLE__) || defined(__QNXNTO__)
#include <dlfcn.h>

#define L2DBUS_MOD_EXTENSION    ".so"
#define L2DBUS_DIRSEP '/'

static void *
l2dbus_moduleLoad
    (
    const char* path
    )
{
    return dlopen(path, RTLD_NOW);
}

static void
l2dbus_moduleUnload
    (
    void*   libHnd
    )
{
    dlclose(libHnd);
}
#endif

#if defined(__WIN32__) || defined(_WIN32)
#define  WIN32_LEAN_AND_MEAN
#include <windows.h>

#define L2DBUS_MOD_EXTENSION    ".dll"
#define L2DBUS_DIRSEP '\\'

static void *
l2dbus_moduleLoad
    (
    const char* path
    )
{
    return (void*)LoadLibrary(path);
}

static void
l2dbus_moduleUnload
    (
    void*   libHnd
    )
{
    FreeLibrary((HINSTANCE)libHnd);
}
#endif

#define L2DBUS_LIBPREFIX        "LOADLIB: "
#define L2DBUS_LIBPREFIX_LEN    (9)


void*
l2dbus_moduleRef
    (
    lua_State*  L,
    const char* modName
    )
{
    void* libHnd = NULL;
    const char* libName = lua_pushfstring(L, "%s%s", modName,
                                            L2DBUS_MOD_EXTENSION);
    const char* key;
    const char* name;
    int libNameLen;

    libNameLen = strlen(libName);

    lua_pushnil(L);
    while ( lua_next(L, LUA_REGISTRYINDEX) != 0 )
    {
        if ( LUA_TSTRING == lua_type(L, -2) )
        {
            key = lua_tostring(L, -2);
            if ( 0 == strncmp(L2DBUS_LIBPREFIX, key, L2DBUS_LIBPREFIX_LEN) )
            {
                name = strrchr(key, L2DBUS_DIRSEP);
                if ( (NULL != name) &&
                    (0 == strncmp(name+1, libName, libNameLen)) )
                {
                    libHnd = l2dbus_moduleLoad(key + L2DBUS_LIBPREFIX_LEN);
                    lua_pop(L, 2);
                    break;
                }
            }

        }
        /* Pop the value and keep the key for the next iteration */
        lua_pop(L, 1);
    }

    /* Pop the library name */
    lua_pop(L, 1);

    return libHnd;
}


void
l2dbus_moduleUnref
    (
    void*       modHnd
    )
{
    return l2dbus_moduleUnload(modHnd);
}



