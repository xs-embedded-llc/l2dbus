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
 * @file           l2dbus_introspection.c
 * @author         Glenn Schmottlach
 * @brief          Implementation of introspection interface.
 *===========================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "cdbus/cdbus.h"
#include "l2dbus_compat.h"
#include "l2dbus_trace.h"
#include "l2dbus_debug.h"
#include "l2dbus_types.h"
#include "l2dbus_introspection.h"
#include "l2dbus_interface.h"
#include "l2dbus_core.h"
#include "l2dbus_object.h"
#include "lualib.h"


static int
l2dbus_newIntrospection
    (
    lua_State*  L
    )
{
    l2dbus_Interface* intfUd;

    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Create: introspection"));

    /* Make sure the module is initialized */
    l2dbus_checkModuleInitialized(L);


    intfUd = (l2dbus_Interface*)l2dbus_objectNew(L, sizeof(*intfUd),
                                             L2DBUS_INTERFACE_TYPE_ID);
    L2DBUS_TRACE((L2DBUS_TRC_TRACE, "Introspection userdata=%p", intfUd));

    if ( NULL == intfUd )
    {
        luaL_error(L, "Failed to create introspection userdata!");
    }
    else
    {
        /* Reset the userdata structure */
        l2dbus_callbackInit(&intfUd->cbCtx);

        intfUd->intf = cdbus_introspectNew();

        if ( NULL == intfUd->intf )
        {
            /* Release any references we may still have */
            l2dbus_callbackUnref(L, &intfUd->cbCtx);
            luaL_error(L, "Failed to allocate introspection interface");
        }
        else
        {
            /* Create a (weak) mapping between the interface userdata pointer and
             * itself.
             */
            l2dbus_objectRegistryAdd(L, intfUd, -1);
        }
    }

    return 1;
}


void
l2dbus_openIntrospection
    (
    lua_State*  L
    )
{
    lua_newtable(L);
    lua_pushcfunction(L, l2dbus_newIntrospection);
    lua_setfield(L, -2, "new");
}



