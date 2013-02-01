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
 * @file           l2dbus_alloc.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of general allocation functions.
 *******************************************************************************
 */
#include <string.h>
#include "dbus/dbus.h"
#include "l2dbus_alloc.h"

void*
l2dbus_malloc
    (
    size_t  size
    )
{
    return dbus_malloc(size);
}


void*
l2dbus_calloc
    (
    size_t  nElt,
    size_t  eltSize
    )
{
    return dbus_malloc0(nElt * eltSize);
}


void*
l2dbus_realloc
    (
    void*   memory,
    size_t  bytes
    )
{
    return dbus_realloc(memory, bytes);
}


void
l2dbus_free
    (
    void*   p
    )
{
    if ( NULL != p )
    {
        dbus_free(p);
    }
}


void l2dbus_freeStringArray
    (
    char**    strArray
    )
{
    size_t    idx = 0;

    if ( NULL != strArray )
    {
        while ( NULL != strArray[idx] )
        {
            l2dbus_free(strArray[idx]);
        }
        l2dbus_free(strArray);
    }
}


char*
l2dbus_strDup
    (
    const char*   s
    )
{
    char* p = NULL;

    if ( NULL != s )
    {
        p = l2dbus_malloc(strlen(s) + sizeof(*p));
        if ( NULL != p )
        {
            p[0] = '\0';
            strcat(p, s);
        }
    }
    return p;
}

