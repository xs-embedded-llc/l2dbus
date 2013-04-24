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
 * @file           l2dbus_alloc.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of general allocation functions.
 *===========================================================================
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

